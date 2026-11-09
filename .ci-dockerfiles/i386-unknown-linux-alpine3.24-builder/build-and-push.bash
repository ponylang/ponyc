#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-i386-unknown-linux-alpine3.24-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"
BUILDER="i386-alpine3.24-builder-$(date +%s)"

docker buildx create --use --name "${BUILDER}"
docker buildx build --provenance false --sbom false --platform linux/386 --pull --push -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
