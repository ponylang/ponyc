FROM ponylang/ponyc-ci:ubuntu-16.04-base

ARG CROSS_TRIPLE=arm-unknown-linux-gnueabi
ARG CROSS_CC=arm-linux-gnueabi-gcc
ARG CROSS_CXX=arm-linux-gnueabi-g++
ARG CROSS_CFLAGS=
ARG CROSS_CXXFLAGS=
ARG CROSS_LDFLAGS=
ARG CROSS_BITS=32
ARG CROSS_LINKER=arm-linux-gnueabi-gcc
ARG CROSS_TUNE=cortex-a9

ARG QEMU_VERSION=2.12.0
ARG COMPILER_RELEASE=2018.05

USER root

RUN wget "https://releases.linaro.org/components/toolchain/binaries/6.4-${COMPILER_RELEASE}/arm-linux-gnueabi/gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabi.tar.xz" \
 && tar xJvf gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabi.tar.xz -C /usr/local --strip 1 \
 && arm-linux-gnueabi-gcc --version \
 && rm gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabi.tar.xz \
 && wget https://github.com/multiarch/qemu-user-static/releases/download/v${QEMU_VERSION}/qemu-arm-static -O /usr/bin/qemu-arm-static \
 && chmod +x /usr/bin/qemu-arm-static

RUN apt-get update \
 && apt-get install -y \
  build-essential \
  cmake \
  g++-6

USER pony
WORKDIR /home/pony
