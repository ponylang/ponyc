FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  build-essential \
  g++-6 \
  git \
  libncurses5-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
  curl \
  cmake \
 && apt-get remove -y llvm-6.0 \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

# install a newer cmake
RUN wget https://cmake.org/files/v3.15/cmake-3.15.3-Linux-x86_64.sh \
 && sh cmake-3.15.3-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
