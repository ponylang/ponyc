FROM alpine

RUN apk add --update \
  alpine-sdk \
  binutils-gold \
  libexecinfo-dev \
  coreutils \
  linux-headers \
  cmake \
  python \
  zlib-dev \
  ncurses-dev \
  bash \
  curl

# add user pony in order to not run tests as root
RUN adduser -D -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
