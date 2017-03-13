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

verify-changelog(){
  echo "Building changelog tool..."
  make CC="$CC1" CXX="$CXX1" && sudo make install

  pushd /tmp

  git clone "https://github.com/ponylang/pony-stable"
  cd pony-stable && git checkout tags/0.0.1 && make && sudo make install && cd -

  git clone "https://github.com/ponylang/changelog-tool"
  cd changelog-tool && git checkout tags/0.2.0 && make && sudo make install && cd -

  popd

  changelog-tool verify CHANGELOG.md
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

  echo "Building ponyc packages for deployment..."
  make CC="$CC1" CXX="$CXX1" verbose=1 arch=x86-64 tune=intel package_name="ponyc" package_base_version="$(cat VERSION)" package_iteration="${PACKAGE_ITERATION}" deploy
}

ponyc-build-docs(){
  echo "Installing mkdocs..."
  sudo -H pip install mkdocs

  echo "Building ponyc docs..."
  make CC="$CC1" CXX="$CXX1" docs

  echo "Uploading docs using mkdocs..."
  git remote add gh-token "https://${STDLIB_TOKEN}@github.com/ponylang/stdlib.ponylang.org"
  git fetch gh-token
  git reset gh-token/master
  cd stdlib-docs
  mkdocs gh-deploy -v --clean --remote-name gh-token --remote-branch master
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
    if [[ "$TRAVIS_BRANCH" == "release" && "$TRAVIS_PULL_REQUEST" == "false" && "$FAVORITE_CONFIG" == "yes" ]]
    then
      ponyc-build-packages
      ponyc-build-docs
    else
      ponyc-test
      verify-changelog
    fi
  ;;

  "linux:llvm-config-4.0")
    ponyc-test
  ;;

  "linux:llvm-config-5.0")
    ponyc-test
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

  "osx:llvm-config-4.0")
    export PATH=llvmsym/:$PATH
    ponyc-test
  ;;

  "osx:llvm-config-5.0")
    export PATH=llvmsym/:$PATH
    ponyc-test
  ;;

  *)
    echo "ERROR: An unrecognized OS and LLVM tuple was found! Consider OS: ${TRAVIS_OS_NAME} and LLVM: ${LLVM_CONFIG}"
    exit 1
  ;;

esac
