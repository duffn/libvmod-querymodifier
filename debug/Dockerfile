FROM varnish:7.6.1

USER root

RUN apt-get update && \
    apt-get install --yes \
    build-essential \
    libtool \
    automake \
    python3-docutils \
    autoconf-archive \
    libasan8 \
    gdb \
    procps

WORKDIR /

COPY . /libvmod-querymodifier
RUN cd /libvmod-querymodifier \
	&& ./bootstrap --enable-asan CFLAGS="-g -O0" \
	&& make \
	&& make install
