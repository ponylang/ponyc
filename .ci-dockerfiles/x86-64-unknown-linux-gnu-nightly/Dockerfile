FROM ubuntu:18.04

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  build-essential \
  cmake \
  g++-6 \
  git \
  libncurses5-dev \
  make \
  xz-utils \
  zlib1g-dev \
  python-pip \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && pip install cloudsmith-cli

# add user pony in order to not run tests as root

RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
