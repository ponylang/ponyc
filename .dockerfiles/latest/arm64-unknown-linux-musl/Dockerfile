FROM alpine:3.21

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

ENV PATH="/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update --no-cache \
    clang \
    curl \
    build-base \
    binutils-gold \
    git

RUN mkdir -p /root/.local/share/ponyup

RUN curl --proto '=https' --tlsv1.2 -sSf -L -o "ponyc-arm64-unknown-linux-alpine3.21.tar.gz" "https://dl.cloudsmith.io/public/ponylang/nightlies/raw/versions/latest/ponyc-arm64-unknown-linux-alpine3.21.tar.gz" \
 && tar --strip-components=1 -xvf ponyc-arm64-unknown-linux-alpine3.21.tar.gz -C /root/.local/share/ponyup

RUN curl --proto '=https' --tlsv1.2 -sSf -L -o "corral-arm64-unknown-linux-alpine3.21.tar.gz" "https://dl.cloudsmith.io/public/ponylang/nightlies/raw/versions/latest/corral-arm64-unknown-linux.tar.gz" \
 && tar --strip-components=1 -xvf corral-arm64-unknown-linux-alpine3.21.tar.gz -C /root/.local/share/ponyup

#RUN sh -c "$(curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/latest-release/ponyup-init.sh)" \
# && ponyup update ponyc nightly \
# && ponyup update corral nightly \
# && ponyup update changelog-tool nightly

WORKDIR /src/main

CMD ["ponyc"]
