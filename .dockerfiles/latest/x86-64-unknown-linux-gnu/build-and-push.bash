#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to DockerHub
# and GitHub Container Registery when you run this ***
#

DOCKERFILE_DIR="$(dirname "$0")"

## DockerHub

docker build --pull -t "ponylang/ponyc:latest" "${DOCKERFILE_DIR}"
docker push "ponylang/ponyc:latest"

## GitHub Container Registry

docker build --pull -t "ghcr.io/ponylang/ponyc:latest" "${DOCKERFILE_DIR}"
docker push "ghcr.io/ponylang/ponyc:latest"
