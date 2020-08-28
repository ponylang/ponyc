#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to DockerHub when you run this ***
#

NAME="ponylang/ponyc-ci-x86-64-unknown-linux-centos8-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

docker build --pull -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker push "${NAME}:${TODAY}"
