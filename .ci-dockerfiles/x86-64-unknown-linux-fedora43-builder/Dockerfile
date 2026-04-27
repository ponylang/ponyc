FROM fedora:43

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN dnf install -y \
      binutils-gold \
      clang \
      cmake \
      compiler-rt \
      curl \
      diffutils \
      findutils \
      gcc \
      gcc-c++ \
      git \
      libatomic \
      libstdc++-static \
      lldb \
      make \
      python3-lldb \
      python3-pip \
      systemtap-sdt-devel \
      wget \
      which \
      xz \
      zlib-devel \
 && dnf clean all \
 && pip3 install --break-system-packages cloudsmith-cli

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
