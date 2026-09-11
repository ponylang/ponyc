ARG FROM_TAG=20260908
FROM ghcr.io/ponylang/ponyc-ci-ubuntu26.04-builder:${FROM_TAG}

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

USER root

RUN apt-get update \
 && apt-get remove -y libssl-dev \
 && apt-get autoremove -y \
 && rm -rf /var/lib/apt/lists/*

ARG OPENSSL_VERSION=3.6.2
RUN curl -fsSL "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz" \
      -o /tmp/openssl.tar.gz \
 && tar -xzf /tmp/openssl.tar.gz -C /tmp \
 && cd /tmp/openssl-${OPENSSL_VERSION} \
 && ./config --prefix=/usr/local no-shared \
 && make -j"$(nproc)" \
 && make install_sw \
 && rm -rf /tmp/openssl*

USER pony
WORKDIR /home/pony
