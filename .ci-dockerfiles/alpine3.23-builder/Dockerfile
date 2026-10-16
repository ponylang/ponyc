FROM alpine:3.23

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN apk update \
  && apk upgrade \
  && apk add --update --no-cache \
  alpine-sdk \
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
  py3-lldb \
  libressl \
  libressl-dev

ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
       UPLOADER_ARCH="arm64"; \
     else \
       UPLOADER_ARCH="x86-64"; \
     fi \
  && curl -fsSL "https://github.com/ponylang/cloudsmith-uploader/releases/latest/download/cloudsmith-uploader-${UPLOADER_ARCH}-unknown-linux.tar.gz" \
     | tar -xz -C /usr/local --strip-components=1

# add user pony in order to not run tests as root
RUN adduser -D -u 1001 -s /bin/sh -h /home/pony -g root pony
USER pony
WORKDIR /home/pony
