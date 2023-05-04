#!/bin/bash

set -o errexit

if [[ -z "${MATERIAL_INSIDERS_ACCESS}" ]]; then
  echo -e "\e[31mMaterial Insiders Access API key needs to be set in MATERIAL_INSIDERS_ACCESS."
  echo -e "Exiting.\e[0m"
  exit 1
fi

MIA="${MATERIAL_INSIDERS_ACCESS}"

set -o nounset

#
# *** You should already be logged in to DockerHub when you run this ***
#

DOCKERFILE_DIR="$(dirname "$0")"

# built from ponyc release tag
FROM_TAG=release-alpine
TAG_AS=release
docker build --pull --build-arg FROM_TAG="${FROM_TAG}" \
  --build-arg MATERIAL_INSIDERS_ACCESS="${MIA}" \
  -t ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}" "${DOCKERFILE_DIR}"
docker push ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}"

# built from ponyc latest tag
FROM_TAG=alpine
TAG_AS=latest
docker build --pull --build-arg FROM_TAG="${FROM_TAG}" \
  --build-arg MATERIAL_INSIDERS_ACCESS="${MIA}" \
  -t ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}" "${DOCKERFILE_DIR}"
docker push ghcr.io/ponylang/ponyc-ci-stdlib-builder:"${TAG_AS}"
