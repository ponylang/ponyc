FROM fedora:44

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

RUN dnf install -y \
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
      openssl-devel \
      python3-lldb \
      python3-pip \
      systemtap-sdt-devel \
      wget \
      which \
      xz \
      zlib-devel \
 && dnf clean all

ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
       UPLOADER_ARCH="arm64"; \
     else \
       UPLOADER_ARCH="x86-64"; \
     fi \
  && curl -fsSL "https://github.com/ponylang/cloudsmith-uploader/releases/latest/download/cloudsmith-uploader-${UPLOADER_ARCH}-unknown-linux.tar.gz" \
     | tar -xz -C /usr/local --strip-components=1

# needed for GitHub actions
RUN git config --global --add safe.directory /__w/ponyc/ponyc

# add user pony in order to not run tests as root
RUN useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony
USER pony
WORKDIR /home/pony
