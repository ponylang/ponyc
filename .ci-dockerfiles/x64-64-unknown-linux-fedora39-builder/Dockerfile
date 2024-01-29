FROM fedora:39

RUN dnf install -y clang \
  cmake \
  git \
  make \
  zlib \
  curl \
  python3-pip \
  lldb \
  libstdc++-static \
 && dnf -y autoremove \
 && dnf -y clean all \
 && rm -rf /var/cache/dnf/* \
 && pip3 install cloudsmith-cli

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
