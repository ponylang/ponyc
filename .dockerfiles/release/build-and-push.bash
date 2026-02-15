#!/bin/bash

#
# ** You should already be logged in to GitHub Container Registry when you run
#    this **
#
# The tag name used here (release) must stay in sync with the
# prune-untagged-nightly-images job in build-nightly-image.yml, which inspects
# this tag's manifest to collect child SHAs for skip-shas protection.
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
