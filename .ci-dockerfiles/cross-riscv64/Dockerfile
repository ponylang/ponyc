FROM ponylang/ponyc-ci-x86-64-unknown-linux-ubuntu20.04-builder:20230807

USER root

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
  gdb-multiarch \
  qemu-user \
  gcc-10-riscv64-linux-gnu \
  g++-10-riscv64-linux-gnu \
  lldb \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get -y autoremove --purge \
 && apt-get -y clean \
 && ln -s /usr/riscv64-linux-gnu/lib/* /lib

RUN cmake --version

USER pony
WORKDIR /home/pony
