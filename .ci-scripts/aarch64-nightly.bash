#!/bin/bash

set -e

# Verify ENV is set up correctly
# We validate all that need to be set in case, in an absolute emergency,
# we need to run this by hand. Otherwise the CI environment should
# provide all of these if properly configured
if [[ -z "${CLOUDSMITH_API_KEY}" ]]; then
  echo -e "\e[31mCloudsmith API key needs to be set in CLOUDSMITH_API_KEY."
  echo -e "Exiting.\e[0m"
  exit 1
fi

if [[ -z "${TRIPLE_VENDOR}" ]]; then
  echo -e "\e[31mVendor needs to be set in TRIPLE_VENDOR."
  echo -e "Exiting.\e[0m"
  exit 1
fi

if [[ -z "${TRIPLE_OS}" ]]; then
  echo -e "\e[31mOperating system needs to be set in TRIPLE_OS."
  echo -e "Exiting.\e[0m"
  exit 1
fi

TODAY=$(date +%Y%m%d)

# Compiler target parameters
MACHINE=armv8-a
PROCESSOR=aarch64

# Triple construction
TRIPLE=${MACHINE}-${TRIPLE_VENDOR}-${TRIPLE_OS}

# Build parameters
MAKE_PARALLELISM=8
BUILD_PREFIX=$(mktemp -d)
DESTINATION=${BUILD_PREFIX}/lib/pony
PONY_VERSION="nightly-${TODAY}"

# Asset information
PACKAGE_DIR=$(mktemp -d)
PACKAGE=ponyc-${TRIPLE}

# Cloudsmith configuration
CLOUDSMITH_VERSION=${TODAY}
ASSET_OWNER=ponylang
ASSET_REPO=nightlies
ASSET_PATH=${ASSET_OWNER}/${ASSET_REPO}
ASSET_FILE=${PACKAGE_DIR}/${PACKAGE}.tar.gz
ASSET_SUMMARY="Pony compiler"
ASSET_DESCRIPTION="https://github.com/ponylang/ponyc"

# Build pony installation
echo "Building ponyc installation..."
make configure arch=${PROCESSOR} build_flags=-j${MAKE_PARALLELISM} \
  version="${PONY_VERSION}"
make build version="${PONY_VERSION}"
make install arch=${PROCESSOR} prefix="${BUILD_PREFIX}" symlink=no version="${PONY_VERSION}"

# Package it all up
echo "Creating .tar.gz of ponyc installation..."
pushd "${DESTINATION}" || exit 1
tar -cvzf "${ASSET_FILE}" -- *
popd || exit 1

# Ship it off to cloudsmith
echo "Uploading package to cloudsmith..."
cloudsmith push raw --version "${CLOUDSMITH_VERSION}" \
  --api-key "${CLOUDSMITH_API_KEY}" --summary "${ASSET_SUMMARY}" \
  --description "${ASSET_DESCRIPTION}" ${ASSET_PATH} "${ASSET_FILE}"
