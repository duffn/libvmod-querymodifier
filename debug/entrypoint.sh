#!/bin/sh

export LD_PRELOAD=/usr/lib/aarch64-linux-gnu/libasan.so.8
export ASAN_OPTIONS=halt_on_error=0:detect_leaks=1

varnishd \
	-F \
	-f /etc/varnish/default.vcl \
	-a http=:"${VARNISH_HTTP_PORT:-80}",HTTP \
	-a proxy=:"${VARNISH_PROXY_PORT:-8443}",PROXY \
	-p feature=+http2 \
	-s malloc,"$VARNISH_SIZE"
