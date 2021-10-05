#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to DockerHub when you run this ***
#

NAME="ponylang/ponyc-ci-aarch64-unknown-linux-musl-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

docker buildx  build --platform linux/arm64 --pull -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker push "${NAME}:${TODAY}"
