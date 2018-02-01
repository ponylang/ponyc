#! /bin/bash

set -o errexit
set -o nounset

ponyc-test(){
  echo "Building and testing ponyc..."
  make CC="$CC1" CXX="$CXX1" test-ci
}

ponyc-build-packages(){
  echo "Installing ruby, rpm, and fpm..."
  rvm use 2.2.3 --default
  sudo apt-get install -y rpm
  gem install fpm

  echo "Building ponyc packages for deployment..."
  make CC="$CC1" CXX="$CXX1" verbose=1 arch=x86-64 tune=intel package_name="ponyc" package_base_version="$(cat VERSION)" deploy
}

ponyc-build-docs(){
  echo "Installing mkdocs and offical theme..."
  sudo -H pip install ponylang-mkdocs

  echo "Building ponyc docs..."
  make CC="$CC1" CXX="$CXX1" docs-online

  echo "Uploading docs using mkdocs..."
  git remote add gh-token "https://${STDLIB_TOKEN}@github.com/ponylang/stdlib.ponylang.org"
  git fetch gh-token
  git reset gh-token/master
  cd stdlib-docs
  mkdocs gh-deploy -v --clean --remote-name gh-token --remote-branch master
}
