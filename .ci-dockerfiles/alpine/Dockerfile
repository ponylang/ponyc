FROM alpine

RUN apk update \
  && apk upgrade \
  && apk add --update \
  alpine-sdk \
  binutils-gold \
  libexecinfo-dev \
  libexecinfo-static \
  coreutils \
  linux-headers \
  cmake \
  python \
  git \
  zlib-dev \
  bash \
  curl

# add user pony in order to not run tests as root
RUN adduser -D -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
