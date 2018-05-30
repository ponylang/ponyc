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

# install pcre2
RUN wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2 \
 && tar xvf pcre2-10.21.tar.bz2 \
 && cd pcre2-10.21 \
 && ./configure --prefix=/usr \
 && make install \
 && cd .. \
 && rm -rf pcre2-10.21*

# install a newer cmake, ubuntu's is to old
RUN wget https://cmake.org/files/v3.11/cmake-3.11.2-Linux-x86_64.sh \
 && sh cmake-3.11.2-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
