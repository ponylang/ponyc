FROM ponylang/ponyc-ci:llvm-7.0.1

ARG CROSS_TRIPLE=i686-unknown-linux-gnu
ARG CROSS_CC=gcc-6
ARG CROSS_CXX=g++-6
ARG CROSS_CFLAGS=-m32
ARG CROSS_CXXFLAGS=-m32
ARG CROSS_LDFLAGS=-m32
ARG CROSS_BITS=32
ARG CROSS_LINKER='gcc-6 -m32'
ARG CROSS_TUNE=generic

USER root

RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y software-properties-common \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update \
 && apt-get install -y \
  libc6:i386 \
  libc6-dev:i386 \
  linux-libc-dev:i386 \
  zlib1g:i386 \
  zlib1g-dev:i386 \
  g++-6 \
  g++-6-multilib \
  gcc-6-multilib \
# cleanup
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

USER pony
WORKDIR /home/pony
