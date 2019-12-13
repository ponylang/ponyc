#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to DockerHub when you run this ***
#

DOCKERFILE_DIR="$(dirname "$0")"

docker build --pull -t "ponylang/ponyc:alpine" "${DOCKERFILE_DIR}"
docker push "ponylang/ponyc:alpine"
