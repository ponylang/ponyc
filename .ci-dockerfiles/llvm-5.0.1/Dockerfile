FROM ponylang/ponyc-ci:ubuntu-14.04-base

ENV LLVM_VERSION 5.0.1

USER root

RUN wget -O - http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8.tar.xz \
 | tar xJf - --strip-components 1 -C /usr/local/ clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8

USER pony
WORKDIR /home/pony
