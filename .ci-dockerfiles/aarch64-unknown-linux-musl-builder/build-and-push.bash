#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-aarch64-unknown-linux-musl-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"
BUILDER="aarch64-builder-$(date +%s)"

docker buildx create --use --name "${BUILDER}"
docker buildx build --platform linux/arm64 --pull -t "${NAME}:${TODAY}" --output "type=image,push=true" "${DOCKERFILE_DIR}"
docker buildx stop "${BUILDER}"
