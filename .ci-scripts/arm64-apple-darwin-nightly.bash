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

if [[ -z "${GITHUB_REPOSITORY}" ]]; then
  echo -e "\e[31mName of this repository needs to be set in GITHUB_REPOSITORY."
  echo -e "\e[31mShould be in the form OWNER/REPO, for example:"
  echo -e "\e[31m     ponylang/ponyc"
  echo -e "\e[31mExiting.\e[0m"
  exit 1
fi

if [[ -z "${GITHUB_TOKEN}" ]]; then
  echo -e "\e[31mGITHUB_TOKEN needs to be set for GHCR publishing."
  echo -e "Exiting.\e[0m"
  exit 1
fi

TODAY=$(date +%Y%m%d)

# Compiler target parameters
MACHINE=arm64
PROCESSOR=armv8-a
CPU=apple-m1

# Triple construction
TRIPLE=${MACHINE}-apple-darwin

# Build parameters
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
make configure arch=${PROCESSOR} cpu=${CPU} \
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

# Additionally publish to GHCR as an OCI artifact. Runs after the Cloudsmith
# push (the current primary) and reuses TODAY and ASSET_FILE so both
# destinations get the same date and bytes.
echo "Uploading package to GHCR..."
python3 "$(dirname "$0")/release/ghcr_nightly.py" push ponyc "${TRIPLE}" \
  "${TODAY}" "${ASSET_FILE}"
