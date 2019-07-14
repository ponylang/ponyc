FROM ubuntu:16.04

ENV LLVM_VERSION 7.0.1

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libncurses5-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys "D401AB61 DBE1D0A2" \
 && echo "deb https://dl.bintray.com/pony-language/pony-stable-debian /" | tee -a /etc/apt/sources.list \
 && apt-get update \
 && apt-get install -y \
  pony-stable \
 && rm -rf /var/lib/apt/lists/* \
 && wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04

WORKDIR /src/ponyc
COPY Makefile LICENSE VERSION /src/ponyc/
COPY benchmark /src/ponyc/benchmark
COPY src      /src/ponyc/src
COPY lib      /src/ponyc/lib
COPY test     /src/ponyc/test
COPY packages /src/ponyc/packages

RUN make arch=x86-64 tune=intel \
 && make install \
 && rm -rf /src/ponyc/build

WORKDIR /src/main

CMD ponyc
