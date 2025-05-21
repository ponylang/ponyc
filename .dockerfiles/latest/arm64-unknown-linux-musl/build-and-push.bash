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
docker buildx build --platform linux/arm64 --pull -t "${NAME}" --output "type=image,push=true" "${DOCKERFILE_DIR}"
docker buildx stop "${BUILDER}"
