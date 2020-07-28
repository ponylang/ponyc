FROM ubuntu:18.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
  apt-transport-https \
  build-essential \
  clang \
  curl \
  git \
  make \
  python3-pip \
  python3-setuptools \
  xz-utils \
  zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && pip3 install cloudsmith-cli

# install a newer cmake
RUN curl --output cmake-3.15.3-Linux-x86_64.sh https://cmake.org/files/v3.15/cmake-3.15.3-Linux-x86_64.sh \
 && sh cmake-3.15.3-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
