#!/usr/bin/env python3
"""Compare the built VMOD to a small, independent raw-query-string model.

Run after `make`: python3 test_querymodifier_generated.py --seed 2026 --cases 200
Requires varnishtest and the locally built src/.libs/libvmod_querymodifier library.
This is deterministic generated testing, not a replay of production traffic.
"""

import argparse
import random
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
NAMES = ("a", "b", "tag", "page", "x[]", "%74ag", "A", "z", "")
VALUES = ("", "1", "2", "a=b", "%20", "0", "hello")
MODES = ("include", "sorted", "sortall", "exclude", "excludeall", "removeall")


def reference(url: str, names: str, mode: str) -> str:
    base, separator, query = url.partition("?")
    if not separator:
        return url
    if mode in ("removeall", "excludeall") or (not names and mode != "sortall"):
        return base

    allowed = set(filter(None, names.split(",")))
    parts = []
    for part in query.split("&"):
        if not part:
            continue
        name, equals, value = part.partition("=")
        if mode == "sortall" or (name in allowed) == (mode != "exclude"):
            parts.append((name, name + ("=" + value if equals and value else "")))

    if mode in ("sorted", "sortall"):
        parts.sort(key=lambda entry: entry[0])  # stable for repeated names
    return base + ("?" + "&".join(part for _, part in parts) if parts else "")


def generate(seed: int, count: int) -> list[tuple[str, str, str]]:
    rng = random.Random(seed)
    cases = [
        ("/feed/", "tag,page", "sorted"),
        ("/feed/?", "tag,page", "sorted"),
        ("/feed/?tag=&page=2&tag=one&z=3", "tag,page", "sorted"),
        ("/feed/?z=3&&=empty&tag=first&tag=second&", "tag,z", "sorted"),
        ("/feed/?tag=first&tag=second&page=2", "tag,page", "sorted"),
        ("/feed/?z=3&tag=first&tag=second&page=2", "", "sortall"),
        ("/feed/?page=2&tag=first&tag=second", "tag,page", "sorted"),
        ("/feed/?%74ag=encoded&tag=raw", "%74ag,tag", "sorted"),
        ("/feed/?a=1&a=2", "a", "exclude"),
        ("/feed/?a=1&z=2", "", "include"),
        ("/feed/?a=1&z=2", "a", "removeall"),
    ]
    for _ in range(count):
        chunks = []
        for _ in range(rng.randrange(0, 22)):
            name = rng.choice(NAMES)
            value = rng.choice(VALUES)
            chunks.append(name + ("=" + value if rng.randrange(2) else ""))
        # Include empty segments, repeats, encoded names, and dangling separators.
        query = "&".join(chunks)
        if rng.randrange(4) == 0:
            query += "&"
        url = "/feed/" + ("?" + query if rng.randrange(8) else "")
        names = ",".join(rng.sample(NAMES[:-1], rng.randrange(0, 6)))
        cases.append((url, names, rng.choice(MODES)))
    return cases


def quoted(value: str) -> str:
    # Generated alphabet is deliberately safe for varnishtest quoted strings.
    assert '"' not in value and "\\" not in value and "\n" not in value
    return '"' + value + '"'


def vtc(cases: list[tuple[str, str, str]]) -> str:
    lines = [
        'varnishtest "Generated querymodifier differential cases"',
        "server s1 {",
    ]
    for url, names, mode in cases:
        lines.extend(("  rxreq", f"  expect req.url == {quoted(reference(url, names, mode))}", "  txresp"))
    lines.extend((
        "} -start",
        "varnish v1 -vcl+backend {",
        "  import querymodifier;",
        "  sub vcl_recv {",
        '    if (req.http.X-Mode == "sortall") {',
        "      set req.url = querymodifier.sortparams(req.url);",
        '    } else if (req.http.X-Mode == "sorted") {',
        "      set req.url = querymodifier.includeparams_sorted(req.url, req.http.X-Names);",
        '    } else if (req.http.X-Mode == "include") {',
        "      set req.url = querymodifier.includeparams(req.url, req.http.X-Names);",
        '    } else if (req.http.X-Mode == "exclude") {',
        "      set req.url = querymodifier.excludeparams(req.url, req.http.X-Names);",
        '    } else if (req.http.X-Mode == "excludeall") {',
        "      set req.url = querymodifier.excludeallparams(req.url);",
        "    } else {",
        "      set req.url = querymodifier.removeallquerystring(req.url);",
        "    }",
        "    return (pass);",
        "  }",
        "} -start",
        "client c1 {",
    ))
    for url, names, mode in cases:
        lines.append(f"  # {mode} {url} names={names}")
        lines.append(f"  txreq -url {quoted(url)} -hdr {quoted('X-Mode: ' + mode)}"
                     + (f" -hdr {quoted('X-Names: ' + names)}" if names else ""))
        lines.extend(("  rxresp", "  expect resp.status == 200"))
    lines.append("} -run")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=2026)
    parser.add_argument("--cases", type=int, default=200)
    parser.add_argument("--vmod-path", type=Path, default=ROOT / "src" / ".libs")
    args = parser.parse_args()
    if args.cases < 0:
        parser.error("--cases must be non-negative")
    cases = generate(args.seed, args.cases)
    # varnishtest keeps a bounded log per test; use small batches to avoid
    # exhausting it before an assertion has a chance to report the failure.
    with tempfile.TemporaryDirectory(prefix="querymodifier-") as temporary:
        for batch, start in enumerate(range(0, len(cases), 25)):
            path = Path(temporary) / f"generated-{batch}.vtc"
            path.write_text(vtc(cases[start:start + 25]))
            command = ["varnishtest", "-p", f"vmod_path={args.vmod_path.resolve()}", str(path)]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode:
                failures = [line for line in result.stdout.splitlines() if " failed" in line or "ERROR" in line]
                print("\n".join(failures[-10:]) or result.stdout[-4000:])
                # Keep the exact failing input for debugging; the seed also reproduces it.
                failure = ROOT / f"querymodifier-failure-{args.seed}-{batch}.vtc"
                failure.write_text(path.read_text())
                raise SystemExit(f"Batch {batch} failed (seed={args.seed}); VTC: {failure}")
    print(f"Passed {len(cases)} generated cases (seed={args.seed})")


if __name__ == "__main__":
    main()
