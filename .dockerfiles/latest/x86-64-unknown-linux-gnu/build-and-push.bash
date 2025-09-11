#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GitHub Container Registry when you run
#     this ***
#

DOCKERFILE_DIR="$(dirname "$0")"

## GitHub Container Registry

docker build --pull -t "ghcr.io/ponylang/ponyc:latest" "${DOCKERFILE_DIR}"
docker push "ghcr.io/ponylang/ponyc:latest"
