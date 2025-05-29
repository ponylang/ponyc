FROM ubuntu:24.04

# Keep annoying tzdata prompt from coming up
# Thanks cmake!
ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN=true

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
  apt-transport-https \
  build-essential \
  clang \
  cmake \
  git \
  libclang-rt-dev \
  lldb \
  make \
  xz-utils \
  zlib1g-dev \
  curl \
  python3-pip \
  wget \
  systemtap-sdt-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && pip3 install --break-system-packages cloudsmith-cli

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
