FROM debian:jessie

RUN echo 'deb http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main' \
  | tee -a /etc/apt/sources.list \
 && echo 'deb-src http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main' \
  | tee -a /etc/apt/sources.list

RUN apt-get update \
 && apt-get install -y wget bzip2 make g++ git \
                       zlib1g-dev libncurses5-dev libssl-dev

RUN wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | apt-key add - \
 && apt-get update \
 && apt-get install -y llvm-3.8-dev

RUN cd /tmp \
 && wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2 \
 && tar xvf pcre2-10.21.tar.bz2 \
 && cd pcre2-10.21 \
 && ./configure --prefix=/usr \
 && make && make install \
 && rm -rf /tmp/*

WORKDIR /src/ponyc
COPY Makefile LICENSE VERSION /src/ponyc/
COPY src      /src/ponyc/src
COPY lib      /src/ponyc/lib
COPY test     /src/ponyc/test
COPY packages /src/ponyc/packages

RUN make config=release clean && make config=release && make install

RUN mkdir /src/main
WORKDIR   /src/main

CMD ponyc
