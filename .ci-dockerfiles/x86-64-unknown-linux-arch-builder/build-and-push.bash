#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

DOCKERFILE_DIR="$(dirname "$0")"
NAME="ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-arch-builder"

# with latest as tag
TAG_AS=latest
docker build -t "${NAME}:${TAG_AS}" "${DOCKERFILE_DIR}"
docker push "${NAME}:${TAG_AS}"
