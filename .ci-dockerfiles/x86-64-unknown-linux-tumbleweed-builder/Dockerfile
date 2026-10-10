FROM opensuse/tumbleweed:latest

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN sed -i 's|http://download.opensuse.org|https://download.opensuse.org|g' \
      /etc/zypp/repos.d/*.repo \
 && zypper --non-interactive dup \
 && zypper --non-interactive install --no-recommends \
      clang \
      cmake \
      diffutils \
      findutils \
      gcc-c++ \
      git \
      libopenssl-devel \
      lldb \
      llvm-gold \
      make \
      python3 \
      python3-lldb \
      shadow \
      zlib-devel \
 && zypper clean --all

RUN useradd -u 1001 -ms /bin/sh -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
