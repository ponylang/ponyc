#!/bin/bash

#
# ** You should already be logged in to GitHub Container Registry when you run
#    this **
#

set -o errexit
set -o nounset

# no unset variables allowed from here on out
# allow above so we can display nice error messages for expected unset variables
set -o nounset

DOCKERFILE_DIR="$(dirname "$0")"

# Build and push "release" tag e.g. ponylang/ponyup:release
NAME="ghcr.io/ponylang/ponyc:release"
BUILDER="ponyc-release-builder-$(date +%s)"
docker buildx create --use --name "${BUILDER}"
docker buildx build --provenance false --sbom false --platform linux/arm64,linux/amd64 --pull --push -t "${NAME}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
