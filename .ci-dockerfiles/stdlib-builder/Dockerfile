ARG FROM_TAG=release-alpine
FROM ghcr.io/ponylang/ponyc:${FROM_TAG}

ARG MATERIAL_INSIDERS_ACCESS=fail

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
  && pip3 install --break-system-packages git+https://${MATERIAL_INSIDERS_ACCESS}@github.com/squidfunk/mkdocs-material-insiders.git
