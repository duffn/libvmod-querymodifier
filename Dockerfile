ARG VARNISH_VERSION=7.7
FROM varnish:${VARNISH_VERSION}

USER root

RUN apt-get update && apt-get install -y \
	build-essential \
	libtool \
	automake \
	autoconf-archive \
	python3-docutils

WORKDIR /
COPY . .

RUN ./bootstrap \
	&& make \
	&& make install
