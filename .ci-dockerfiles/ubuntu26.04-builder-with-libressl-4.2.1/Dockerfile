ARG FROM_TAG=20260908
FROM ghcr.io/ponylang/ponyc-ci-ubuntu26.04-builder:${FROM_TAG}

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

USER root

RUN apt-get update \
 && apt-get remove -y libssl-dev \
 && apt-get autoremove -y \
 && rm -rf /var/lib/apt/lists/*

ARG LIBRESSL_VERSION=4.2.1
RUN curl -fsSL "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-${LIBRESSL_VERSION}.tar.gz" \
      -o /tmp/libressl.tar.gz \
 && tar -xzf /tmp/libressl.tar.gz -C /tmp \
 && cd /tmp/libressl-${LIBRESSL_VERSION} \
 && ./configure --prefix=/usr/local --disable-shared --enable-static \
 && make -j"$(nproc)" \
 && make install \
 && rm -rf /tmp/libressl*

USER pony
WORKDIR /home/pony
