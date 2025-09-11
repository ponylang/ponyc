FROM ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-ubuntu24.04-builder:20240425

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

ARG CROSS_TRIPLE=arm-unknown-linux-gnueabihf
ARG CROSS_CC=arm-linux-gnueabihf-gcc
ARG CROSS_CXX=arm-linux-gnueabihf-g++
ARG CROSS_CFLAGS=
ARG CROSS_CXXFLAGS=
ARG CROSS_LDFLAGS=
ARG CROSS_BITS=32
ARG CROSS_LINKER=arm-linux-gnueabihf-gcc
ARG CROSS_TUNE=cortex-a7

USER root

RUN curl -L --output gcc-linearo.tar.xz "https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz" \
 && tar xJf gcc-linearo.tar.xz -C /usr/local --strip 1 \
 && arm-linux-gnueabihf-gcc --version \
 && rm gcc-linearo.tar.xz \
 && curl -L --output /usr/bin/qemu-arm-static https://github.com/multiarch/qemu-user-static/releases/download/v6.1.0-8/qemu-arm-static \
 && chmod +x /usr/bin/qemu-arm-static \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
  lldb \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

RUN cmake --version

USER pony
WORKDIR /home/pony
