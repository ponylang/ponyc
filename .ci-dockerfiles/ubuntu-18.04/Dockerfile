FROM ubuntu:18.04

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  clang \
  llvm \
  libncurses5-dev \
  libpcre2-dev \
  libssl-dev \
  cmake \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
  python

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
