FROM alpine:3.12

ENV PATH "/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update --no-cache \
    binutils-gold \
    build-base \
    clang \
    curl \
    git \
    libexecinfo-dev \
    libexecinfo-static

RUN sh -c "$(curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/latest-release/ponyup-init.sh)" \
 && ponyup update ponyc nightly --platform=musl\
 && ponyup update stable nightly \
 && ponyup update corral nightly \
 && ponyup update changelog-tool nightly

WORKDIR /src/main

CMD ["ponyc"]
