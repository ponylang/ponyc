#! /bin/bash

set -o errexit
set -o nounset

if [[ "$TRAVIS_BRANCH" == "release" && "$FAVORITE_CONFIG" != "yes" ]]
then
  echo "This is a release branch and there's nothing this matrix element must do."
  exit 0
fi

ponyc-test(){
  echo "Building and testing ponyc..."
  make CC="$CC1" CXX="$CXX1" test-ci
}

ponyc-build-packages(){
  echo "Installing ruby, rpm, and fpm..."
  rvm use 2.2.3 --default
  sudo apt-get install -y rpm
  gem install fpm

  # The PACKAGE_ITERATION will be fed to the DEB and RPM systems by FPM
  # as a suffix to the base version (DEB:debian_revision or RPM:release,
  # used to disambiguate packages with the same version).
  PACKAGE_ITERATION="${TRAVIS_BUILD_NUMBER}.$(git rev-parse --short --verify 'HEAD^{commit}')"

  echo "Removing any existing ponyc installs before creating for deployment..."
  make CC="$CC1" CXX="$CXX1" clean
  echo "Building ponyc packages for deployment..."
  make CC="$CC1" CXX="$CXX1" verbose=1 arch=x86-64 tune=intel package_name="ponyc" package_base_version="$(cat VERSION)" package_iteration="${PACKAGE_ITERATION}" deploy
}

ponyc-build-docs(){
  echo "Installing mkdocs..."
  sudo -H pip install mkdocs

  echo "Removing any existing ponyc installs before creating for doc generation..."
  make clean
  echo "Building ponyc docs..."
  make CC="$CC1" CXX="$CXX1" docs

  echo "Fetching out the gh-pages branch..."
  git remote add gh-token "https://${GH_TOKEN}@github.com/${TRAVIS_REPO_SLUG}"
  git fetch gh-token && git fetch gh-token gh-pages:gh-pages

  echo "Uploading docs using mkdocs..."
  cd stdlib-docs
  mkdocs gh-deploy -v --clean --remote-name gh-token
}

case "${TRAVIS_OS_NAME}:${LLVM_CONFIG}" in

  "linux:llvm-config-3.7")
    ponyc-test
  ;;

  "linux:llvm-config-3.8")
    ponyc-test
  ;;

  "linux:llvm-config-3.9")
    # when FAVORITE_CONFIG stops matching part of this case, move this logic
    if [[ "$TRAVIS_BRANCH" == "release" && "$FAVORITE_CONFIG" == "yes" ]]
    then
      ponyc-build-packages
      ponyc-build-docs
    else
      ponyc-test
    fi
  ;;

  "osx:llvm-config-3.7")
    ponyc-test
  ;;

  "osx:llvm-config-3.8")
    ponyc-test
  ;;

  "osx:llvm-config-3.9")
    export PATH=llvmsym/:$PATH
    ponyc-test
  ;;

  *)
    echo "ERROR: An unrecognized OS and LLVM tuple was found! Consider OS: ${TRAVIS_OS_NAME} and LLVM: ${LLVM_CONFIG}"
    exit 1
  ;;

esac
