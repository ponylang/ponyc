FROM ubuntu:14.04

RUN apt-get update \
 && apt-get install -y software-properties-common \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update \
 && apt-get install -y \
  apt-transport-https \
  build-essential \
  g++-6 \
  git \
  libncurses5-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
  python

# install a newer cmake
RUN wget https://cmake.org/files/v3.11/cmake-3.11.2-Linux-x86_64.sh \
 && sh cmake-3.11.2-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# install newer git to support submodules
RUN apt-get install -y gettext libcurl3-dev libexpat-dev zlib1g-dev \
  && git clone https://github.com/git/git git-src \
  && cd git-src \
  && git checkout v2.17.1 \
  && make -j$(nproc) prefix=/usr all \
  && make prefix=/usr install \
  && rm -rf git-src

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
