ARG FROM_TAG=release
FROM ghcr.io/ponylang/ponyc:${FROM_TAG}

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN apk update \
  && apk upgrade \
  && apk add --update --no-cache \
  bash \
  git-fast-import \
  libffi \
  libffi-dev \
  libressl \
  libressl-dev \
  python3 \
  python3-dev \
  py3-pip \
  tar \
  && pip3 install --upgrade --break-system-packages pip \
  && pip3 install --break-system-packages mkdocs \
  && pip3 install --break-system-packages mkdocs-material
