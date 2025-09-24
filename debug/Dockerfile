FROM varnish:8.0.0

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
    procps \
    && dpkg -i /pkgs/*varnish-dev*.deb || apt-get -f install -y

WORKDIR /

COPY . /libvmod-querymodifier
RUN cd /libvmod-querymodifier \
	&& ./bootstrap --enable-asan CFLAGS="-g -O0" \
	&& make \
	&& make install
