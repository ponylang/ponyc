FROM ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-ubuntu24.04-builder:20260908

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

USER root

# The stdlib's net package links against libssl/libcrypto, so the cross build
# needs riscv64 versions. Ubuntu's riscv64 packages live on ports.ubuntu.com;
# pin the existing amd64 sources so apt doesn't try to fetch riscv64 from
# archive.ubuntu.com (which 404s), then add a ports entry for riscv64.
RUN dpkg --add-architecture riscv64 \
 && sed -i '/^Types:/a Architectures: amd64' /etc/apt/sources.list.d/ubuntu.sources \
 && printf 'Types: deb\nURIs: http://ports.ubuntu.com/ubuntu-ports\nSuites: noble noble-updates noble-security\nComponents: main universe\nArchitectures: riscv64\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n' \
      > /etc/apt/sources.list.d/riscv64-ports.sources \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
  gdb-multiarch \
  qemu-user \
  gcc-10-riscv64-linux-gnu \
  g++-10-riscv64-linux-gnu \
  lldb \
  libssl-dev:riscv64 \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && ln -sf /usr/riscv64-linux-gnu/lib/* /lib \
 && ln -s /usr/lib/riscv64-linux-gnu/libssl.* /usr/riscv64-linux-gnu/lib/ \
 && ln -s /usr/lib/riscv64-linux-gnu/libcrypto.* /usr/riscv64-linux-gnu/lib/

RUN cmake --version

USER pony
WORKDIR /home/pony
