#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GitHub Container Registry when you run
#     this ***
#

DOCKERFILE_DIR="$(dirname "$0")"
NAME="ghcr.io/ponylang/ponyc:alpine-arm64"
BUILDER="arm64-builder-$(date +%s)"

docker buildx create --use --name "${BUILDER}"
docker buildx build --platform linux/arm64 --pull --push -t "${NAME}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
