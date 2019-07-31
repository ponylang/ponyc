FROM alpine

ENV LLVM_VERSION 7

RUN apk add --update \
  alpine-sdk \
  binutils-gold \
  llvm${LLVM_VERSION} \
  llvm${LLVM_VERSION}-dev \
  llvm${LLVM_VERSION}-static \
  libexecinfo-dev \
  libexecinfo-static \
  coreutils \
  linux-headers \
  cmake \
  git \
  zlib-dev \
  bash \
  curl

# add user pony in order to not run tests as root
RUN adduser -D -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
