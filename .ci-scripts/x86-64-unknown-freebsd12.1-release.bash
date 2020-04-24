#!/bin/bash

set -e

API_KEY=$1
if [[ ${API_KEY} == "" ]]
then
  echo "API_KEY needs to be supplied as first script argument."
  exit 1
fi


# Compiler target parameters
ARCH=x86-64
PIC=true

# Triple construction
VENDOR=unknown
OS=freebsd12.1
TRIPLE=${ARCH}-${VENDOR}-${OS}

# Build parameters
MAKE_PARALLELISM=8
BUILD_PREFIX=$(mktemp -d)
DESTINATION=${BUILD_PREFIX}/lib/pony

# Asset information
PACKAGE_DIR=$(mktemp -d)
PACKAGE=ponyc-${TRIPLE}

# Cloudsmith configuration
CLOUDSMITH_VERSION=$(cat VERSION)
ASSET_OWNER=ponylang
ASSET_REPO=releases
ASSET_PATH=${ASSET_OWNER}/${ASSET_REPO}
ASSET_FILE=${PACKAGE_DIR}/${PACKAGE}.tar.gz
ASSET_SUMMARY="Pony compiler"
ASSET_DESCRIPTION="https://github.com/ponylang/ponyc"

# Build pony installation
echo "Building ponyc installation..."
gmake configure arch=${ARCH} build_flags=-j${MAKE_PARALLELISM}
gmake build arch=${ARCH} build_flags=-j${MAKE_PARALLELISM}
gmake install prefix=${BUILD_PREFIX} symlink=no arch=${ARCH} \
  build_flags=-j${MAKE_PARALLELISM}

# Package it all up
echo "Creating .tar.gz of ponyc installation..."
pushd ${DESTINATION} || exit 1
tar -cvzf ${ASSET_FILE} *
popd || exit 1

# Ship it off to cloudsmith
echo "Uploading package to cloudsmith..."
cloudsmith push raw --version "${CLOUDSMITH_VERSION}" --api-key ${API_KEY} \
  --summary "${ASSET_SUMMARY}" --description "${ASSET_DESCRIPTION}" \
  ${ASSET_PATH} ${ASSET_FILE}
