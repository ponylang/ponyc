FROM alpine:3.18


RUN apk update \
  && apk upgrade \
  && apk add --update --no-cache \
  alpine-sdk \
  binutils-gold \
  clang \
  clang-dev \
  coreutils \
  linux-headers \
  cmake \
  git \
  zlib-dev \
  bash \
  curl \
  py3-pip \
  gdb \
  lldb \
&& pip install cloudsmith-cli

# add user pony in order to not run tests as root
RUN adduser -D -u 1001 -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
