FROM ubuntu:18.04

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  build-essential \
  g++-6 \
  git \
  libncurses5-dev \
  libssl-dev \
  libpcre2-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

# add user pony in order to not run tests as root

RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
