ARG FROM_TAG=release-alpine
FROM ponylang/ponyc:${FROM_TAG}

ARG MATERIAL_INSIDERS_ACCESS="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"

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
  && pip3 install --upgrade pip \
  && pip3 install mkdocs \
  && pip3 install git+https://${MATERIAL_INSIDERS_ACCESS}@github.com/squidfunk/mkdocs-material-insiders.git
