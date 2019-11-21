FROM ubuntu:18.04

ENV PATH "/root/.pony/ponyup/bin:$PATH"

RUN apt-get update \
 && apt-get install -y \
    curl \
    g++

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update ponyc release --libc=gnu \
 && ponyup update stable release \
 && ponyup update corral release \
 && ponyup update changelog-tool release

WORKDIR /src/main

CMD ponyc
