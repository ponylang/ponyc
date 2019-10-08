#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to DockerHub when you run this ***
#

TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

docker build -t "ponylang/ponyc-ci-x86-64-unknown-linux-gnu-nightly:${TODAY}" \
  "${DOCKERFILE_DIR}"
docker push "ponylang/ponyc-ci-x86-64-unknown-linux-gnu-nightly:${TODAY}"
