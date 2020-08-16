#!/bin/bash

set -o errexit
set -o nounset

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

STDLIB_TOKEN=$1
if [[ ${STDLIB_TOKEN} == "" ]]
then
  echo "STDLIB_TOKEN needs to be supplied as first script argument."
  exit 1
fi

ponyc packages/stdlib --docs-public --pass expr
sed -i 's/site_name:\ stdlib/site_name:\ Pony Standard Library/' stdlib-docs/mkdocs.yml

echo "Uploading docs using mkdocs..."
git remote add gh-token "https://${STDLIB_TOKEN}@github.com/ponylang/stdlib.ponylang.io"
git fetch gh-token
git reset gh-token/master
pushd stdlib-docs
mkdocs gh-deploy -v --clean --remote-name gh-token --remote-branch master
popd
