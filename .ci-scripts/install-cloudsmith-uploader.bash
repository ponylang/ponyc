#!/bin/bash

set -o errexit

TRIPLE="${1}"

if [[ -z "${TRIPLE}" ]]; then
  echo "Usage: install-cloudsmith-uploader.bash <triple>"
  echo "Example: install-cloudsmith-uploader.bash arm64-apple-darwin"
  exit 1
fi

curl -sL -o /tmp/cloudsmith-uploader.tar.gz \
  "https://github.com/ponylang/cloudsmith-uploader/releases/latest/download/cloudsmith-uploader-${TRIPLE}.tar.gz"
tar -xzf /tmp/cloudsmith-uploader.tar.gz -C /tmp
cp /tmp/*/bin/cloudsmith-uploader /tmp/cloudsmith-uploader
