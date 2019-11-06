FROM alpine:3.10

ENV PATH "/root/.pony/ponyup/bin:$PATH"

RUN apk add --update \
    curl

RUN curl -s --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/master/ponyup-init.sh | sh \
 && ponyup update nightly --libc=musl

CMD ponyc
