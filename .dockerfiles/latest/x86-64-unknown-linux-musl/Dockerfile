FROM alpine:3.11

ENV PATH "/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update \
    binutils-gold \
    build-base \
    clang \
    curl \
    git \
    libexecinfo-dev \
    libexecinfo-static

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update ponyc nightly --platform=musl\
 && ponyup update stable nightly \
 && ponyup update corral nightly \
 && ponyup update changelog-tool nightly

WORKDIR /src/main

CMD ponyc
