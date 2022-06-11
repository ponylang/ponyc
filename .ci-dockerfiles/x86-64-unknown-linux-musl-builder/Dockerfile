FROM alpine:3.16

RUN apk update \
  && apk upgrade \
  && apk add --update --no-cache \
  alpine-sdk \
  binutils-gold \
  clang \
  clang-dev \
  libexecinfo-dev \
  libexecinfo-static \
  coreutils \
  linux-headers \
  cmake \
  git \
  zlib-dev \
  bash \
  curl \
  py3-pip \
&& pip install cloudsmith-cli

# add user pony in order to not run tests as root
RUN adduser -D -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
