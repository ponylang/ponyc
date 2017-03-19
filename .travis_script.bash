#! /bin/bash

set +o errexit
set +o nounset
set +o

if [[ "$TRAVIS_BRANCH" == "release" ]]
then
  if [[ "$config" != "release" || "${TRAVIS_OS_NAME}" != "linux" || "${LLVM_CONFIG}" != "llvm-config-3.9" ]]
  then
    echo "This is a release branch and there's nothing this matrix element must do."
    exit 0
  fi
fi

case "${TRAVIS_OS_NAME}:${LLVM_CONFIG}" in

  "linux:llvm-config-3.7")
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "linux:llvm-config-3.8")
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "linux:llvm-config-3.9")
    if [[ "$TRAVIS_BRANCH" == "release" && "$config" == "release" ]]
    then
      # install ruby, rpm, and fpm
      rvm use 2.2.3 --default
      sudo apt-get install -y rpm
      gem install fpm

      # Build packages for deployment
      #
      # The PACKAGE_ITERATION will be fed to the DEB and RPM systems by FPM
      # as a suffix to the base version (DEB:debian_revision or RPM:release,
      # used to disambiguate packages with the same version).
      PACKAGE_ITERATION="${TRAVIS_BUILD_NUMBER}.$(git rev-parse --short --verify 'HEAD^{commit}')"
      make verbose=1 arch=x86-64 tune=intel config=release package_name="ponyc" package_base_version="$(cat VERSION)" package_iteration="${PACKAGE_ITERATION}" deploy

      # Checkout the gh-pages branch
      git remote add gh-token "https://${GH_TOKEN}@github.com/${TRAVIS_REPO_SLUG}"
      git fetch gh-token && git fetch gh-token gh-pages:gh-pages

      # Build documentation
      build/release/ponyc packages/stdlib --docs

      # Upload documentation on gh-pages
      cd stdlib-docs
      sudo -H pip install mkdocs
      cp ../.docs/extra.js docs/
      sed -i '' 's/site_name:\ stdlib/site_name:\ Pony Standard Library/' mkdocs.yml
      mkdocs gh-deploy -v --clean --remote-name gh-token
    else
      make CC="$CC1" CXX="$CXX1" test-ci
    fi
  ;;

  "osx:llvm-config-3.7")
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "osx:llvm-config-3.8")
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "osx:llvm-config-3.9")
    export PATH=llvmsym/:$PATH
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  *)
    echo "ERROR: An unrecognized OS and LLVM tuple was found! Consider OS: ${TRAVIS_OS_NAME} and LLVM: ${LLVM_CONFIG}"
    exit 1
  ;;

esac
