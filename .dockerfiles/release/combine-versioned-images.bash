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

set -o nounset

NAME="ghcr.io/ponylang/ponyc"

sources=()

# function to check if an image exists
check_image() {
  local image="$1"
  if docker manifest inspect "$image" > /dev/null 2>&1; then
    echo "Image exists: $image"
    sources+=("$image")
  else
    echo "Image not found: $image"
  fi
}

merge_images() {
  local TAG="$1"
  echo "Checking available architecture images for ${NAME}:$TAG"

  check_image "${NAME}:${TAG}-amd64"
  check_image "${NAME}:${TAG}-arm64"

  if [ ${#sources[@]} -eq 0 ]; then
    echo "No images found for merging, skipping."
    return
  fi

  echo "Creating or updating manifest tag: ${NAME}:${TAG}"

  # Attempt to inspect existing manifest
  if docker manifest inspect "${NAME}:${TAG}" >/dev/null 2>&1; then
    echo "Existing manifest found, updating it."
    docker manifest create --amend "${NAME}:${TAG}" "${sources[@]}"
  else
    echo "No existing manifest found, creating new one."
    docker manifest create "${NAME}:${TAG}" "${sources[@]}"
  fi
  docker manifest push "${NAME}:${TAG}"

  echo "Manifest created or updated successfully."
}

merge_images "${VERSION}"
