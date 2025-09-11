FROM alpine:3.20

ENV PATH="/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update --no-cache \
    clang \
    curl \
    build-base \
    binutils-gold \
    git

RUN sh -c "$(curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/latest-release/ponyup-init.sh)" \
 && ponyup update ponyc release \
 && ponyup update corral release \
 && ponyup update changelog-tool release

WORKDIR /src/main

CMD ["ponyc"]
