FROM centos:centos8

RUN yum install -y \
    dnf-plugins-core \
    epel-release \
  && yum config-manager --set-enabled PowerTools

RUN yum install -y \
    clang \
    diffutils \
    git \
    libatomic \
    libstdc++-static \
    make \
    python3-pip \
    zlib-devel \
  && pip3 install cloudsmith-cli

# install a newer cmake
RUN curl --output cmake-3.15.3-Linux-x86_64.sh https://cmake.org/files/v3.15/cmake-3.15.3-Linux-x86_64.sh \
 && sh cmake-3.15.3-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
