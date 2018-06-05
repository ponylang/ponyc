FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libncurses5-dev \
  libpcre2-dev \
  cmake \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
  python

 RUN wget https://www.openssl.org/source/openssl-1.1.0.tar.gz \
   && tar xf openssl-1.1.0.tar.gz \
   && cd openssl-1.1.0 \
   && ./config \
   && make \
   && make install \
   && cd .. \
   && rm -rf openssl-1.1.0*

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
