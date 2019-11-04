FROM alpine:3.9

ENV LLVM_VERSION 5

RUN apk update \
  && apk upgrade \
  && apk add --update \
  alpine-sdk \
  binutils-gold \
  llvm${LLVM_VERSION}-static \
  llvm${LLVM_VERSION}-dev \
  libexecinfo-dev \
  coreutils \
  linux-headers \
  cmake \
  git \
  zlib-dev \
  ncurses-dev \
  curl \
  bash

WORKDIR /src/ponyc
COPY Makefile LICENSE VERSION /src/ponyc/
COPY benchmark      /src/ponyc/benchmark
COPY src            /src/ponyc/src
COPY lib/blake2     /src/ponyc/lib/blake2
COPY lib/gbenchmark /src/ponyc/lib/gbenchmark
COPY test           /src/ponyc/test
COPY packages       /src/ponyc/packages

RUN make install arch=x86-64 tune=intel default_pic=true

RUN git clone https://github.com/ponylang/pony-stable /src/pony-stable

WORKDIR /src/pony-stable

RUN make \
 && make install

WORKDIR /src/main

RUN rm -rf /src/ponyc /src/pony-stable

CMD ponyc
