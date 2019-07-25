FROM ponylang/ponyc-ci:llvm-7.0.1

ARG CROSS_TRIPLE=arm-unknown-linux-gnueabihf
ARG CROSS_CC=arm-linux-gnueabihf-gcc
ARG CROSS_CXX=arm-linux-gnueabihf-g++
ARG CROSS_CFLAGS=
ARG CROSS_CXXFLAGS=
ARG CROSS_LDFLAGS=
ARG CROSS_BITS=32
ARG CROSS_LINKER=arm-linux-gnueabihf-gcc
ARG CROSS_TUNE=cortex-a7

ARG QEMU_VERSION=2.12.0
ARG COMPILER_RELEASE=2018.05

USER root

RUN wget "https://releases.linaro.org/components/toolchain/binaries/6.4-${COMPILER_RELEASE}/arm-linux-gnueabihf/gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabihf.tar.xz" \
 && tar xJvf gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabihf.tar.xz -C /usr/local --strip 1 \
 && arm-linux-gnueabihf-gcc --version \
 && rm gcc-linaro-6.4.1-${COMPILER_RELEASE}-x86_64_arm-linux-gnueabihf.tar.xz \
 && wget https://github.com/multiarch/qemu-user-static/releases/download/v${QEMU_VERSION}/qemu-arm-static -O /usr/bin/qemu-arm-static \
 && chmod +x /usr/bin/qemu-arm-static

USER pony
WORKDIR /home/pony
