#!/bin/bash

set -o errexit
set -o nounset

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

ponyc packages/stdlib --docs-public --pass expr
sed -i 's/site_name:\ stdlib/site_name:\ Pony Standard Library/' stdlib-docs/mkdocs.yml

echo "Building docs using mkdocs..."
pushd stdlib-docs
mkdocs build
popd
