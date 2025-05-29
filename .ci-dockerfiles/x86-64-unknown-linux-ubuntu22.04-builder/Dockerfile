FROM ubuntu:22.04

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
 && pip3 install cloudsmith-cli

## lldb-14 is buggy, we need to get 17.
RUN echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-17 main" >> /etc/apt/sources.list \
 && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
 && apt-get update \
 && apt-get install -y --no-install-recommends lldb-17 \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && ln -s /usr/lib/llvm-17/bin/lldb /usr/bin

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
