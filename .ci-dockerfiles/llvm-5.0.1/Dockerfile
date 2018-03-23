FROM ubuntu:16.04

ENV LLVM_VERSION 5.0.1

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libunwind-dev \
  libncurses5-dev \
  libpcre2-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
 && wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-16.04

# delete the libunwind files shipped with LLVM, they conflict with the system libunwind files
RUN rm /usr/local/lib/libunwind*
