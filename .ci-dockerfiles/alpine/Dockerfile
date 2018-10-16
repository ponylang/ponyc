FROM alpine

RUN apk add --update \
  alpine-sdk \
  libressl-dev \
  binutils-gold \
  pcre2-dev \
  libexecinfo-dev \
  coreutils \
  linux-headers \
  cmake \
  python

# add user pony in order to not run tests as root
RUN adduser -D -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
