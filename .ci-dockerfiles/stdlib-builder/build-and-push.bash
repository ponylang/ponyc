#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GitHub Container Registry when you run
#     this ***
#

DOCKERFILE_DIR="$(dirname "$0")"

# built from ponyc release tag
FROM_TAG=release
TAG_AS=release
docker build --pull --build-arg FROM_TAG="${FROM_TAG}" \
  -t ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}" "${DOCKERFILE_DIR}"
docker push ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}"

# built from ponyc nightly tag
FROM_TAG=nightly
TAG_AS=nightly
docker build --pull --build-arg FROM_TAG="${FROM_TAG}" \
  -t ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}" "${DOCKERFILE_DIR}"
docker push ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}"
