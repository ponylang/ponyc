#!/bin/bash

VERSION=39

set -o errexit
set -o nounset

#
# *** You should already be logged in to GHCR when you run this ***
#

NAME="ghcr.io/ponylang/ponyc-ci-x86-64-unknown-linux-fedora39-builder"
TODAY=$(date +%Y%m%d)
DOCKERFILE_DIR="$(dirname "$0")"

# Create a container
CONTAINER=$(buildah from scratch)

# Mount the container filesystem
MOUNTPOINT=$(buildah mount $CONTAINER)

# Install a basic filesystem and minimal set of packages, and httpd
dnf install -y --installroot $MOUNTPOINT  --releasever $VERSION \
	cmake            \
	git              \
	make             \
	zlib             \
	curl             \
	lldb             \
	libstdc++-static \
	--nodocs --setopt install_weak_deps=0

# Install Cloudsmith from host
PYTHON=$(ls "$MOUNTPOINT/usr/bin" | grep "python3\.")
target="$MOUNTPOINT/usr/local/lib/$PYTHON/site-packages"
local="$MOUNTPOINT/home/root/.local/bin"
mkdir -p "$target" "$local"
pip3 install --target="$target" cloudsmith-cli
cat <<EOF > $local/cloudsmith
#!/usr/bin/python3
# -*- coding: utf-8 -*-
import re
import sys
from cloudsmith_cli.cli.commands.main import main
if __name__ == '__main__':
    sys.argv[0] = re.sub(r'(-script\.pyw|\.exe)?$', '', sys.argv[0])
    sys.exit(main())
EOF
chmod +x "$local/cloudsmith"

# Cleanup
dnf -y autoremove --installroot $MOUNTPOINT
dnf -y clean all --installroot $MOUNTPOINT --releasever $VERSION
rm -rf "$MOUNTPOINT/var/cache/dnf"
buildah unmount $CONTAINER

buildah config --cmd "git config --global--add safe.directory /__w/ponyc/ponyc" $CONTAINER
buildah config --cmd "useradd -u 1001 -ms /bin/bash -d /home/pony -g root pony" $CONTAINER

# buildah config --user pony $CONTAINER
buildah config --workingdir /home/pony $CONTAINER

# Save the container to an image
buildah commit --squash $CONTAINER "${NAME}:${TODAY}"

podman push "${NAME}:${TODAY}"
