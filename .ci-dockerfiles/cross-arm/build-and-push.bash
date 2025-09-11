#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-cross-arm"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

docker build --pull -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker push "${NAME}:${TODAY}"
