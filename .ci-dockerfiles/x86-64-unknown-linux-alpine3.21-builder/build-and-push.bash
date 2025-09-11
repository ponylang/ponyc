#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

docker build --pull -t "ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-alpine3.21-builder:${TODAY}" \
  "${DOCKERFILE_DIR}"
docker push "ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-alpine3.21-builder:${TODAY}"
