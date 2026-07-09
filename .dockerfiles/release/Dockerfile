# Keep this Alpine version in sync with the build-release-image workflow gate and
# the trigger-release-tasks.bash dispatch names. ponyup inside the container
# detects this base's Alpine version and fetches the matching ponyc package, so a
# gate naming a different version fires the build off a package ponyup won't
# fetch. No CI check compares the gate against this base; a mismatch surfaces
# only during a real release image build.
FROM alpine:3.24

LABEL org.opencontainers.image.source="https://github.com/ponylang/ponyc"

ENV PATH="/root/.local/share/ponyup/bin:$PATH"

RUN apk add --update --no-cache \
    clang \
    curl \
    build-base \
    git

RUN sh -c "$(curl --proto '=https' --tlsv1.2 -sSf https://raw.githubusercontent.com/ponylang/ponyup/latest-release/ponyup-init.sh)" \
 && ponyup --api-timeout 30 update ponyc release --retries 3 \
 && ponyup --api-timeout 30 update corral release --retries 3 \
 && ponyup --api-timeout 30 update changelog-tool release --retries 3

WORKDIR /src/main

CMD ["ponyc"]
