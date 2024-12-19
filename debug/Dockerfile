FROM varnish:7.6.1

USER root

RUN apt-get update && \
    apt-get install --yes \
    build-essential \
    libtool \
    automake \
    python3-docutils \
    autoconf-archive \
    libasan8

WORKDIR /

COPY . /libvmod-querymodifier
RUN cd /libvmod-querymodifier \
	&& ./bootstrap --enable-asan \
	&& make \
	&& make install
