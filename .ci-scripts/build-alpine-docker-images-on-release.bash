#!/bin/bash

#
# *** You should already be logged in to DockerHub when you run this ***
#

set -o errexit
set -o nounset

# Gather expected arguments.
if [ $# -lt 1 ]
then
  echo "Tag is required"
  exit 1
fi

# We aren't validating TAG is in our x.y.z format but we could.
# For now, TAG validating is left up to the configuration in
# .circleci/config.yml.
TAG=$1

# Build and push :TAG tag e.g. ponyc:0.32.1-alpine.
DOCKER_TAG=ponylang/ponyc:"${TAG}"-alpine
docker build --file=.dockerhub/alpine/Dockerfile -t "${DOCKER_TAG}" .
docker push "${DOCKER_TAG}"

# Build and push "release" tag e.g. ponyc:release-alpine
DOCKER_TAG=ponylang/ponyc:release-alpine
docker build --file=.dockerhub/alpine/Dockerfile -t "${DOCKER_TAG}" .
docker push "${DOCKER_TAG}"
