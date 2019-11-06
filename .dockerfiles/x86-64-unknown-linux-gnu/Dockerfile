FROM ubuntu:18.04

ENV PATH "/root/.pony/ponyup/bin:$PATH"

RUN apt-get update \
 && apt-get install -y \
    curl

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update nightly --libc=gnu

CMD ponyc
