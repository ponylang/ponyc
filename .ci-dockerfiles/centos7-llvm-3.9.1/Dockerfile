FROM centos:7

ENV LLVM_VERSION 3.9.1

RUN yum install -y \
    yum-plugin-copr \
    which \
    git \
    gcc-c++ \
    make \
    file \
    openssl-devel \
    pcre2-devel \
    zlib-devel \
    ncurses-devel \
    libatomic \
 && yum copr enable -y alonid/llvm-${LLVM_VERSION} \
 && yum install -y \
    llvm-${LLVM_VERSION} \
    llvm-${LLVM_VERSION}-devel \
    llvm-${LLVM_VERSION}-static

# add user pony in order to not run tests as root
RUN useradd -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
