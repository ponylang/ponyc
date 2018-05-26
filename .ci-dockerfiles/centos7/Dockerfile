FROM centos:7

RUN yum install -y \
    yum-plugin-copr \
    which \
    git \
    gcc-c++ \
    make \
    openssl-devel \
    pcre2-devel \
    zlib-devel \
    ncurses-devel \
    libatomic \
    wget

RUN wget https://cmake.org/files/v3.10/cmake-3.10.3-Linux-x86_64.sh \
 && sh cmake-3.10.3-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
