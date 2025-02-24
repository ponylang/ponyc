FROM archlinux:base

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN pacman -Syyu --noconfirm \
 && pacman -S --noconfirm \
 clang \
 cmake \
 git \
 lldb \
 make

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/sh -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
