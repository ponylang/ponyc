ARG FROM_DISTRO=ubuntu
ARG FROM_VERSION=xenial
FROM ${FROM_DISTRO}:${FROM_VERSION}
ARG LLVM_VERSION=7.0.1

RUN apt-get update \
 && apt-get install -y \
  apt-transport-https \
  ca-certificates \
  dirmngr \
  gnupg \
 # && apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys "47E4F8DEE04F0923" \
 # && echo deb https://dl.bintray.com/pony-language/ponylang-debian ${FROM_VERSION} main | tee -a /etc/apt/sources.list \
 # && apt-get update \
 # && apt-get install -y \
  devscripts \
  build-essential \
  lintian \
  debhelper \
  equivs \
  git \
  cmake \
  ninja \
 && git clone --depth 1 --branch llvmorg-${LLVM_VERSION} https://github.com/llvm/llvm-project.git \
 && cd llvm-project/ \
 && mkdir build \
 && cd build\
 && cmake -DCMAKE_BUILD_TYPE=Release ../llvm/ \
 && cmake --build . \
 && cmake --build . --target install \
 && cd ../../ \
 && rm -rf llvm-project \
 && apt-get remove -y \
  cmake \
  ninja

WORKDIR /home/pony

