FROM alpine:3.9

ENV LLVM_VERSION 5

RUN apk add --update --no-cache \
  alpine-sdk \
  libressl-dev \
  binutils-gold \
  llvm${LLVM_VERSION} \
  llvm${LLVM_VERSION}-dev \
  pcre2-dev \
  libexecinfo-dev \
  coreutils \
  linux-headers \
  cmake \
  git

WORKDIR /src/ponyc
COPY Makefile LICENSE VERSION /src/ponyc/
COPY benchmark /src/ponyc/benchmark
COPY src      /src/ponyc/src
COPY lib      /src/ponyc/lib
COPY test     /src/ponyc/test
COPY packages /src/ponyc/packages

RUN make install arch=x86-64 tune=intel default_pic=true

RUN git clone https://github.com/ponylang/pony-stable /src/pony-stable

WORKDIR /src/pony-stable

RUN make \
 && make install

WORKDIR /src/main

RUN rm -rf /src/ponyc
RUN rm -rf /src/pony-stable

CMD ponyc
