# DOESNT WORK - INSTALLS 4.0.0
# 4.0.1 isnt available as a prebuilt that works on any docker image I create

FROM ubuntu:16.04

ENV LLVM_VERSION 4.0.1

RUN echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-4.0 main" | tee -a /etc/apt/sources.list

RUN echo "deb-src http://apt.llvm.org/xenial/ llvm-toolchain-xenial-4.0 main" | tee -a /etc/apt/sources.list

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  g++ \
  git \
  libncurses5-dev \
  libpcre2-dev \
  libssl-dev \
  make \
  wget \
  xz-utils \
  zlib1g-dev \
  llvm-4.0


CMD hostname
