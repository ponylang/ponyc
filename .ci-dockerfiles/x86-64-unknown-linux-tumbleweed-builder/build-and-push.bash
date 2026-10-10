#!/bin/bash

set -o errexit
set -o nounset

DOCKERFILE_DIR="$(dirname "$0")"
NAME="ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-tumbleweed-builder"
TAG_AS=latest

docker build --pull --no-cache -t "${NAME}:${TAG_AS}" "${DOCKERFILE_DIR}"
docker push "${NAME}:${TAG_AS}"
