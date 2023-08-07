FROM ubuntu:20.04

# Keep annoying tzdata prompt from coming up
# Thanks cmake!
ENV DEBIAN_FRONTEND noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
  apt-transport-https \
  build-essential \
  clang \
  git \
  make \
  xz-utils \
  zlib1g-dev \
  curl \
  python3-pip \
  lldb \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && pip3 install cloudsmith-cli

 # install a version of cmake that will meet out needs
RUN curl --output cmake-3.25.1-linux-x86_64.sh https://cmake.org/files/v3.25/cmake-3.25.1-linux-x86_64.sh \
 && sh cmake-3.25.1-linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
