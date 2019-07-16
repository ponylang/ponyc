FROM ubuntu:16.04

ENV LLVM_VERSION 7.0.1

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libncurses5-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04

 RUN wget https://www.openssl.org/source/openssl-1.1.0.tar.gz \
   && tar xf openssl-1.1.0.tar.gz \
   && cd openssl-1.1.0 \
   && ./config \
   && make \
   && make install \
   && cd .. \
   && rm -rf openssl-1.1.0*

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
