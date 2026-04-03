FROM alpine:3.23

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

ENV PATH="/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update --no-cache \
    clang \
    curl \
    build-base \
    binutils-gold \
    git

RUN sh -c "$(curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/latest-release/ponyup-init.sh)" \
 && ponyup --api-timeout 30 update ponyc release --retries 3 \
 && ponyup --api-timeout 30 update corral release --retries 3 \
 && ponyup --api-timeout 30 update changelog-tool release --retries 3

WORKDIR /src/main

CMD ["ponyc"]
