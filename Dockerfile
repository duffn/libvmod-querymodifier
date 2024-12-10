ARG VARNISH_VERSION=7.6
FROM varnish:${VARNISH_VERSION}

USER root

RUN apt-get update && apt-get install -y \
	build-essential \
	libtool \
	automake \
	python3-docutils

WORKDIR /
COPY . .

RUN ./bootstrap \
	&& make \
	&& make install
