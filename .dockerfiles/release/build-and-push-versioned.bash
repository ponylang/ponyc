#!/bin/bash

#
# *** You should already be logged in to GitHub Container Registry when you run
#     this ***
#

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

ARCH=$(uname -m)
case "${ARCH}" in
  x86_64)
    ARCH_TAG="amd64"
    ;;
  aarch64|arm64)
    ARCH_TAG="arm64"
    ;;
  *)
    echo "Error: Unsupported architecture '${ARCH}'" >&2
    exit 1
    ;;
esac

DOCKERFILE_DIR="$(dirname "$0")"
BUILDER="ponyc-versioned-release-builder-$(date +%s)"
NAME="ghcr.io/ponylang/ponyc:${VERSION}-${ARCH_TAG}"

echo "Building ${VERSION}-${ARCH_TAG} image"
docker buildx create --use --name "${BUILDER}"
docker buildx build --provenance false --sbom false --pull --push \
  --build-arg -t "${NAME}" "${DOCKERFILE_DIR}"
docker buildx rm "${BUILDER}"
