#!/bin/env bash
#
# Run this inside Haiku VM guest to create ponyc package file.
# Reads BUILD_PREFIX, PACKAGE and PACKAGE_DIR from the environment.
# Reads VERSION from VERSION file, ARCH from `uname -m` if they're
# not available in environment.
# Defaults to PACKAGE_REVISION = 1 it it's not set in environment.
# Creates $PACKAGE_DIR/$PACKAGE.hpkg file with content of $BUILD_PREFIX directory.
# Generated package contains info about license, summary and full description, etc...
# They should be kept in sync with info from README.md.
set -euo pipefail

: ${BUILD_PREFIX:?set BUILD_PREFIX path pointing to where the distributed files can be found}
: ${PACKAGE:?set PACKAGE name, e.g., ponyc-0.67.0-1-x86_64, as is customary for Haiku packages}
: ${PACKAGE_DIR:?set PACKAGE_DIR pointing to target directory for generated package file}

VERSION=${VERSION:=$(cat VERSION)}
ARCH=${ARCH:=$(uname -m)}
PACKAGE_REVISION=${PACKAGE_REVISION:=1}
HAIKU_REQUIREMENT=$(ls /boot/system/packages|grep haiku-|sed 's/-[^-]*\.hpkg//'|sed 's/-/ >= /')
LIBEXECINFO_REQUIREMENT=$(ls /boot/system/packages|grep libexecinfo-|sed 's/-[^-]*\.hpkg//'|sed 's/-/ >= /')

# Create .PackageInfo file
cat >"$PACKAGE_DIR/.PackageInfo" <<EOF
name          Ponyc
version       $VERSION-$PACKAGE_REVISION
architecture  $ARCH
summary       "Reference compiler for the Pony programming language"
description   "Pony is an open-source, object-oriented, actor-model, capabilities-secure, high-performance programming language.

Pony is still pre-1.0 and as such, semi-regularly introduces breaking changes. These changes are usually fairly easy to adapt to. Applications written in Pony are currently used in production environments."
packager      "Builder ponyc"
vendor        "GitHub"
licenses {
	"BSD (2-clause)"
}
copyrights {
	"2016-2020 The Pony Developers"
	"2014-2015 Causality Ltd."
}
provides {
	ponyc = $VERSION
	cmd:ponyc = $VERSION
	cmd:ponyc = $VERSION
	cmd:pony_doc = $VERSION
	cmd:pony_lsp = $VERSION
	cmd:pony_lint = $VERSION
	lib:libponyc = $VERSION
	lib:libponyrt = $VERSION
	lib:libponyc_standalone = $VERSION
}
requires {
	$HAIKU_REQUIREMENT
	$LIBEXECINFO_REQUIREMENT
#	cmake >= 4.1.6
#	python3.14 >= 3.14.6
#	libexecinfo_devel >= 1.1.6
}
urls {
	"https://www.ponylang.io"
}
source-urls {
	"https://github.com/ponylang/ponyc/archive/refs/tags/$VERSION.zip"
}
EOF

# Create symlinks - executables
mkdir -p "$BUILD_PREFIX/bin"
for f in `find $BUILD_PREFIX/lib/pony/**/bin/* -maxdepth 0 -xtype f` ; do
	ln --force --relative --symbolic "$f" "$BUILD_PREFIX"/bin/$(basename $f)
done

# Create symlinks - libraries
for f in `find $BUILD_PREFIX/lib/pony/**/lib/* -maxdepth 1 -xtype f` ; do
	ln --force --relative --symbolic "$f" "$BUILD_PREFIX"/lib/$(basename $f)
done

# Create symlinks - directories with header files
mkdir -p "$BUILD_PREFIX/develop/headers"
for d in `find $BUILD_PREFIX/lib/pony/**/include/* -maxdepth 0 -xtype d` ; do
	ln --force --relative --symbolic "$d" "$BUILD_PREFIX"/develop/headers/$(basename $d)
done

package create -i "$PACKAGE_DIR/.PackageInfo" -C "$BUILD_PREFIX" "$PACKAGE_DIR/$PACKAGE.hpkg"
