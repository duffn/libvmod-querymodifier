ARG VARNISH_VERSION=8.0.2
FROM varnish:${VARNISH_VERSION}-alpine

USER root
RUN apk add --no-cache \
    build-base \
    git \
    libtool \
    automake \
    autoconf \
    autoconf-archive \
    py3-docutils \
    python3 \
    varnish-dev

WORKDIR /work
COPY . .

# Regenerate the VMOD interface after configure: bootstrap's first pass can
# emit an invalid version identifier when building without git metadata.
RUN ./bootstrap \
    && rm -f src/vcc_querymodifier_if.c src/vcc_querymodifier_if.h \
    && make \
    && make check \
    && python3 test_querymodifier_generated.py --seed 2026 --cases 200 \
    && make install
