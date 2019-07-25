#!/bin/bash

# Build parameters
ARCH=x86-64
PIC=true

# More
MAKE_PARALLELISM=4
BUILD_PREFIX=/tmp/pony
DESTINATION=${BUILD_PREFIX}/lib/pony

# Triple construction
VENDOR=unknown
OS=linux-gnu
TRIPLE=${ARCH}-${VENDOR}-${OS}

#
PACKAGE_DIR=/tmp
PACKAGE=ponyc_${TRIPLE}

# Cloudsmith configuration
VERSION=$(date +%Y%m%d)
API_KEY=1cc732891a064c6542e3632a6ee281b69dbdce18
ASSET_OWNER=main-pony
ASSET_REPO=pony-nightlies
ASSET_PATH=${ASSET_OWNER}/${ASSET_REPO}
ASSET_FILE=${PACKAGE_DIR}/${PACKAGE}.tar.gz
ASSET_SUMMARY=Pony compiler
ASSET_DESCRIPTION=https://github.com/ponylang/ponyc


# Build pony installation
make install prefix=${BUILD_PREFIX} default_pic=${PIC} arch=${ARCH} \
  -j${MAKE_PARALLELISM} -f Makefile-lib-llvm symlink=no use=llvm_link_static

# Package it all up
tar -cvzf ${ASSET_FILE} ${DESTINATION}/*

# Ship it off to cloudsmith
cloudsmith push raw --version "${VERSION}" --api-key ${API_KEY} \
  --summary "${ASSET_SUMMARY}" --description "${ASSET_DESCRIPTION}" \
  ${ASSET_PATH} ${ASSET_FILE}
