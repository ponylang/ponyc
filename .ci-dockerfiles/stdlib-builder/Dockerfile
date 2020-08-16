ARG FROM_TAG=release-alpine
FROM ponylang/ponyc:${FROM_TAG}

RUN apk update \
  && apk upgrade \
  && apk add --update \
  bash \
  git-fast-import \
  libffi \
  libffi-dev \
  libressl \
  libressl-dev \
  python3 \
  python3-dev \
  py3-pip \
  && pip3 install --upgrade pip \
  && pip3 install wheel \
  && pip3 install mkdocs \
  && pip3 install mkdocs-ponylang
