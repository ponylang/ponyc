FROM ponylang/ponyc-ci:ubuntu-14.04-base

ENV LLVM_VERSION 5.0.2

USER root

RUN wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-14.04.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-14.04 \
   && chown -R root:root /usr/local

USER pony
WORKDIR /home/pony
