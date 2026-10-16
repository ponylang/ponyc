FROM ubuntu:26.04

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

# Keep annoying tzdata prompt from coming up
# Thanks cmake!
ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN=true

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
  apt-transport-https \
  build-essential \
  clang \
  cmake \
  git \
  libclang-rt-dev \
  lldb \
  make \
  xz-utils \
  zlib1g-dev \
  curl \
  python3-pip \
  wget \
  libssl-dev \
  systemtap-sdt-dev \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean

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
