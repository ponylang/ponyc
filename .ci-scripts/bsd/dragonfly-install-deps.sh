#!/bin/sh
set -e

# Image libraries can be too old for binaries from the package repository.
pkg upgrade -y
pkg install -y cmake gmake git python3 cxx_atomics rsync gcc13 ca_root_nss pcre2
pkg clean -ay
git config --global --add safe.directory /build/ponyc
