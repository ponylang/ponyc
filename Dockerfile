FROM ubuntu:16.04

ENV LLVM_VERSION 3.9.1

RUN apt-get update \
 && apt-get install -y \
  g++ \
  git \
  libncurses5-dev \
  libpcre2-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* \
 && wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04

WORKDIR /src/ponyc
COPY Makefile LICENSE VERSION /src/ponyc/
COPY src      /src/ponyc/src
COPY lib      /src/ponyc/lib
COPY test     /src/ponyc/test
COPY packages /src/ponyc/packages

RUN make \
 && make install \
 && rm -rf /src/ponyc/build

WORKDIR /src/main

CMD ponyc
