FROM alpine:3.11

ENV PATH "/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update \
    curl \
    build-base \
    binutils-gold \
    libexecinfo-dev \
    libexecinfo-static \
    git

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update ponyc nightly --platform=musl\
 && ponyup update stable nightly \
 && ponyup update corral nightly \
 && ponyup update changelog-tool nightly

WORKDIR /src/main

CMD ponyc
