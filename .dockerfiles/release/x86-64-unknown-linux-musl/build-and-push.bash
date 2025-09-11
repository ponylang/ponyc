#!/bin/bash

# ** You should already be logged in to GitHub Container Registry when you run
#    this **
#
# Builds docker release images with two tags:
#
# - release-alpine
# - X.Y.Z-alpine for example 0.32.1-alpine
#
# Tools required in the environment that runs this:
#
# - bash
# - docker

set -o errexit

# Verify ENV is set up correctly
# We validate all that need to be set in case, in an absolute emergency,
# we need to run this by hand. Otherwise the GitHub actions environment should
# provide all of these if properly configured
if [[ -z "${VERSION}" ]]; then
  echo -e "\e[31mThe version number needs to be set in VERSION."
  echo -e "\e[31mThe tag should be in the following form:"
  echo -e "\e[31m    X.Y.Z"
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

if [[ -z "${GITHUB_REPOSITORY}" ]]; then
  echo -e "\e[31mName of this repository needs to be set in GITHUB_REPOSITORY."
  echo -e "\e[31mShould be in the form OWNER/REPO, for example:"
  echo -e "\e[31m     ponylang/ponyup"
  echo -e "\e[31mThis will be used as the docker image name."
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

# no unset variables allowed from here on out
# allow above so we can display nice error messages for expected unset variables
set -o nounset

DOCKERFILE_DIR="$(dirname "$0")"

## GitHub Container Registry

# Build and push :VERSION tag e.g. ponylang/ponyup:0.32.1-alpine
DOCKER_TAG="ghcr.io/${GITHUB_REPOSITORY}:${VERSION}-alpine"
docker build --pull -t "${DOCKER_TAG}" "${DOCKERFILE_DIR}"
docker push "${DOCKER_TAG}"

# Build and push "release" tag e.g. ponylang/ponyup:release-alpine
DOCKER_TAG="ghcr.io/${GITHUB_REPOSITORY}:release-alpine"
docker build --pull -t "${DOCKER_TAG}" "${DOCKERFILE_DIR}"
docker push "${DOCKER_TAG}"
