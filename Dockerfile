FROM ubuntu:16.04

RUN apt-get update \
 && apt-get install -y make g++ git \
                       zlib1g-dev libncurses5-dev libssl-dev \
                       llvm-dev libpcre2-dev \
 && rm -rf /var/lib/apt/lists/*

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
