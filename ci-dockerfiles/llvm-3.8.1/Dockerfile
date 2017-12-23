FROM ubuntu:16.04

ENV LLVM_VERSION 3.8.1

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libncurses5-dev \
  libpcre2-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04

CMD hostname
