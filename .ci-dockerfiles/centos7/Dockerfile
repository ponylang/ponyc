FROM centos:7

RUN yum install -y \
    yum-plugin-copr \
    which \
    git \
    gcc-c++ \
    make \
    zlib-devel \
    ncurses-devel \
    libatomic \
    wget

# install a newer cmake
RUN wget https://cmake.org/files/v3.11/cmake-3.11.2-Linux-x86_64.sh \
 && sh cmake-3.11.2-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# install newer git to support submodules
RUN yum install -y gettext libcurl-devel expat-devel zlib-devel \
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
