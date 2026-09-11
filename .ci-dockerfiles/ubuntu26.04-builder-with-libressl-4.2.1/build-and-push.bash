#!/bin/bash

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-ubuntu26.04-builder-with-libressl-4.2.1"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"
BUILDER="ssl-libressl421-$(date +%s)"

docker buildx create --use --name "${BUILDER}"
docker buildx build --provenance false --sbom false --platform linux/amd64 --pull --push -t "${NAME}:${TODAY}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
