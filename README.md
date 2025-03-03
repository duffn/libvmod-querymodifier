# libvmod-querymodifier

This is a simple Varnish VMOD that allows modification of a URL's query parameters by including or excluding specified parameters and their values.

## Status

ℹ️ This VMOD is currently exploratory and being actively developed and tested. It has not been run in any production environment yet,
so you should not yet use this in a production environment. 

If you need to manipulate querystrings in production, you should currently explore [`libvmod-queryfilter`](https://github.com/nytimes/libvmod-queryfilter/) or [`vmod-querystring`](https://git.sr.ht/~dridi/vmod-querystring).

If instead you just want to contribute to a friendly VMOD repository, continue on!

## Usage

### Inclusion

List the parameters that you would like to have _remain_ in the URL. All other query parameters and their values will be removed.

```
import querymodifier;

set req.url = querymodifier.modifyparams(url=req.url, params="search,id", exclude_params=false);
# Or use the convenience function, `includeparams`.
set req.url = querymodifier.includeparams(url=req.url, params="search,id");

# Original URL: example.com/?search=name&ts=123456789&id=987654321
# Modified URL: example.com/?search=name&id=987654321
```

### Exclusion

List the parameters that you would like to have _removed_ from the URL. All other query parameters and their values will remain.

```
import querymodifier;

set req.url = querymodifier.modifyparams(url=req.url, params="ts,v", exclude_params=true);
# Or use the convenience function, `excludeparams`.
set req.url = querymodifier.excludparams(url=req.url, params="ts,v");

# Original URL: example.com/?search=name&ts=123456789&v=123456789&id=987654321
# Modified URL: example.com/?search=name&id=987654321
```

### Remove all valid query parameters

Remove all query parameters by passing in an empty string.

```
import querymodifier;
set req.url = querymodifier.modifyparams(url=req.url, params="", exclude_params=true);
# Or use the convenience function, `excludeallparams`.
# set req.url = querymodifier.excludeallparams(url=req.url);

# Original URL: example.com/?search=name&ts=123456789&v=123456789&id=987654321
# Modified URL: example.com/
```

### Remove all query string

Remove all  of the query string, i.e. everything after, and including the `?` regardless of if
the are valid `name=value` query string parameters.

```
import querymodifier;
set req.url = querymodifier.removeallquerystring(url=req.url);

# Original URL: example.com/?123456
# Modified URL: example.com/
```

### Additional

See the tests for more parameter edge cases.

## Building

This module is primarily manually tested with Varnish 7.6, but also includes vtc tests for version 7.5.

```
./bootstrap
make
make check # optionally run tests, recommended.
sudo make install
```

## Contributing

Fork, code, and PR! See build instructions above.

I'm happy to review any PRs. Any bug reports are also welcome.

### Debugging

#### ASan

The module can also be built with [`AddressSanitizer`](https://github.com/google/sanitizers/wiki/AddressSanitizer) support.

It is recommended that when developing on the module, you build with `AddressSanitizer` support enabled in order to help identify any memory issues with the VMOD.

In order to build the module with this enabled, run the `bootstrap` script with `--enable-asan`. 

```
./bootstrap --enable-asan
```

There are also some scripts in the `debug` directory to assist. Navigate to the `debug` directory and run `docker compose up --build` in order to build the module with ASan support as well as with a backend `nginx` to field example requests.

_Note_: Do not use the module built with ASan support in production. This is meant for development purposes only.

#### gdb

`gdb` is also included in the debug Dockerfile for your convenience.

- After you've brought up Docker Compose, exec into the Varnish container.

```bash
docker compose exec varnish
```

- Attach `gdb` to the Varnish child process. You can either get the PID with `ps` or Varnish will print the child PID to the console like `varnish-1  | Debug: Child (31) Started`.

```bash
(gdb) attach 31
```

- Set a breakpoint, for example on the `vmod_modifyparams` function. A `.gdbinit` file is included in the Docker container to instruct `gdb` where to find the VMOD shared libraries.

```bash
(gdb) b vmod_modifyparams
Breakpoint 1 at 0xffff7e0b14cc: file vmod_querymodifier.c, line 219.
```

- Send a request to `http://localhost:8080` that exercises the VMOD.

- Continue the debugger and then use `gdb` as you normally would.

```bash
(gdb) c
Continuing.
[Switching to Thread 0xffff855cf140 (LWP 372)]

Thread 101 "cache-worker" hit Breakpoint 1, vmod_modifyparams (ctx=0xffff855cd9b8, uri=0xffff79a3d8ac "/?blah=1&ts=1", params_in=0xffff7e0e7610 "ts,v,cacheFix,date",
    exclude_params=1) at vmod_querymodifier.c:219
219         CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
```

## Acknowledgements

- The NY Times [`libvmod-queryfilter` VMOD](https://github.com/nytimes/libvmod-queryfilter/) for insipiration.
- dridi [`vmod-querystring` VMOD](https://git.sr.ht/~dridi/vmod-querystring) for insipiration.
- [`vcdk`](https://github.com/nigoroll/vcdk/) for the project structure.
- Guillaume Quintard for the [VMOD tutorial](https://info.varnish-software.com/blog/creating-a-vmod-vmod-str).

## License

[Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0)
