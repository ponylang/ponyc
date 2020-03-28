#!/bin/bash

set -e

API_KEY=$1
if [[ ${API_KEY} == "" ]]; then
  echo "API_KEY needs to be supplied as first script argument."
  exit 1
fi

# Compiler target parameters
ARCH=x86-64

# Triple construction
VENDOR=apple
OS=darwin
TRIPLE=${ARCH}-${VENDOR}-${OS}

# Build parameters
MAKE_PARALLELISM=8
BUILD_PREFIX=$(mktemp -d)
PONY_VERSION=$(cat VERSION)
DESTINATION=${BUILD_PREFIX}/${PONY_VERSION}

# Asset information
PACKAGE_DIR=$(mktemp -d)
PACKAGE=ponyc-${TRIPLE}

# Cloudsmith configuration
CLOUDSMITH_VERSION=${PONY_VERSION}
ASSET_OWNER=ponylang
ASSET_REPO=releases
ASSET_PATH=${ASSET_OWNER}/${ASSET_REPO}
ASSET_FILE=${PACKAGE_DIR}/${PACKAGE}.tar.gz
ASSET_SUMMARY="Pony compiler"
ASSET_DESCRIPTION="https://github.com/ponylang/ponyc"

# Build pony installation
echo "Building ponyc installation..."
MAKE_FLAGS="arch=${ARCH} build_flags=-j${MAKE_PARALLELISM}"
make configure "${MAKE_FLAGS}"
make build "${MAKE_FLAGS}"
make install "DESTDIR=${DESTINATION}" "${MAKE_FLAGS}"

# Package it all up
echo "Creating .tar.gz of ponyc installation..."
pushd "${BUILD_PREFIX}" || exit 1
tar -cvzf "${ASSET_FILE}" "${PONY_VERSION}"
popd || exit 1

# Ship it off to cloudsmith
echo "Uploading package to cloudsmith..."
cloudsmith push raw --version "${CLOUDSMITH_VERSION}" --api-key "${API_KEY}" \
  --summary "${ASSET_SUMMARY}" --description "${ASSET_DESCRIPTION}" \
  "${ASSET_PATH}" "${ASSET_FILE}"
