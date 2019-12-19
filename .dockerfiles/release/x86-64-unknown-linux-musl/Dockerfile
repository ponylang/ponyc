FROM alpine:3.10

ENV PATH "/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update \
    curl \
    build-base \
    binutils-gold \
    libexecinfo-dev \
    libexecinfo-static

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update ponyc release --libc=musl \
 && ponyup update stable release \
 && ponyup update corral release \
 && ponyup update changelog-tool release

WORKDIR /src/main

CMD ponyc
