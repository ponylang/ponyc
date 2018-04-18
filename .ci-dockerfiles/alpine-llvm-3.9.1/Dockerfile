FROM alpine

ENV LLVM_VERSION 3.9

RUN apk add --update \
  alpine-sdk \
  libressl-dev \
  binutils-gold \
  llvm${LLVM_VERSION} \
  llvm${LLVM_VERSION}-dev \
  pcre2-dev \
  libexecinfo-dev \
  coreutils \
  linux-headers

# add user pony in order to not run tests as root
RUN useradd -ms /bin/sh -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
