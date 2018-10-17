#!/bin/bash

set -o errexit
set -o nounset

# include install commands
. .travis_install.bash
# include various build commands
. .travis_commands.bash

# when running a vagrant build of ponyc
if [[ "${VAGRANT_ENV}" != "" ]]
then
  set -x

  case "${VAGRANT_ENV}" in

    "freebsd11-x86_64")
      date
      docker run --rm -u pony:2000 -v $(pwd):/home/pony ponylang/ponyc-ci:cross-llvm-3.9.1-freebsd11-x86_64 make arch=x86-64 config=${config} verbose=1 CC=clang CXX=clang++ CFLAGS="-target x86_64-unknown-freebsd11.1 --sysroot=/opt/cross-freebsd-11/ -isystem /opt/cross-freebsd-11/usr/local/llvm39/include/" CXXFLAGS="-target x86_64-unknown-freebsd11.1 --sysroot=/opt/cross-freebsd-11/ -isystem /opt/cross-freebsd-11/usr/local/llvm39/include/" LDFLAGS="-target x86_64-unknown-freebsd11.1 --sysroot=/opt/cross-freebsd-11/ -isystem /opt/cross-freebsd-11/usr/local/llvm39/include/ -L/opt/cross-freebsd-11/usr/local/llvm39/lib" OSTYPE=bsd use="llvm_link_static" CROSS_SYSROOT=/opt/cross-freebsd-11 -j$(nproc)
      date
      download_vagrant
      qemu-system-x86_64 --version
      date
      sudo vagrant ssh -c "cd /vagrant && ./build/${config}/ponyc --version"
      sudo vagrant ssh -c "cd /vagrant && ./build/${config}/libponyc.tests"
      date
      sudo vagrant ssh -c "cd /vagrant && ./build/${config}/libponyrt.tests"
      date
      sudo vagrant ssh -c "cd /vagrant && PONYPATH=.:${PONYPATH} ./build/${config}/ponyc -d -s --checktree --verify packages/stdlib"
      sudo vagrant ssh -c "cd /vagrant && ./stdlib --sequential && rm ./stdlib"
      date
      sudo vagrant ssh -c "cd /vagrant && PONYPATH=.:${PONYPATH} ./build/${config}/ponyc --checktree --verify packages/stdlib"
      sudo vagrant ssh -c "cd /vagrant && ./stdlib --sequential && rm ./stdlib"
      date
      sudo vagrant ssh -c "cd /vagrant && PONYPATH=.:${PONYPATH} find examples/*/* -name '*.pony' -print | xargs -n 1 dirname  | sort -u | grep -v ffi- | xargs -n 1 -I {} ./build/${config}/ponyc -d -s --checktree -o {} {}"
      date
      sudo vagrant ssh -c "cd /vagrant && ./build/${config}/ponyc --antlr > pony.g.new"
      sudo vagrant ssh -c "cd /vagrant && diff pony.g pony.g.new"
      sudo vagrant ssh -c "cd /vagrant && rm pony.g.new"
      date
    ;;

    *)
      echo "ERROR: An unrecognized vagrant environment was found! VAGRANT_ENV: ${VAGRANT_ENV}"
      exit 1
    ;;

  esac
                                                                                                                        68,1          Bot
  exit
fi

case "${TRAVIS_OS_NAME}" in
  "linux")

    # just for testing the docs building
    ponyc-build-docs

    # when building debian packages for a nightly cron job or manual api requested job to make sure packaging isn't broken
    if [[ "$TRAVIS_BRANCH" == "master" && "$RELEASE_DEBS" != "" && ( "$TRAVIS_EVENT_TYPE" == "cron" || "$TRAVIS_EVENT_TYPE" == "api" ) ]]
    then
      # verify docs build first
      ponyc-build-docs
      # now the packaging
      "ponyc-build-debs-$RELEASE_DEBS" master
      exit
    fi

    # when building appimage package for a nightly cron job or manual api requested job to make sure packaging isn't broken
    if [[ "$TRAVIS_BRANCH" == "master" && "$RELEASE_DEBS" == "" && "$CROSS_ARCH" == "" && ( "$TRAVIS_EVENT_TYPE" == "cron" || "$TRAVIS_EVENT_TYPE" == "api" ) ]]
    then
      build_appimage "$(cat VERSION)"
      exit
    fi

    # when building debian packages for a release
    if [[ "$TRAVIS_BRANCH" == "release" && "$TRAVIS_PULL_REQUEST" == "false" && "$RELEASE_DEBS" != "" ]]
    then
      "ponyc-build-debs-$RELEASE_DEBS" "$(cat VERSION)"
    fi

    # when running a cross build of ponyc
    if [[ "${CROSS_ARCH}" != "" ]]
    then
      set -x
      # build and test for x86_64 first
      echo "Building and testing ponyc..."
      docker run --rm -u pony:2000 -v $(pwd):/home/pony "ponylang/ponyc-ci:cross-llvm-${LLVM_VERSION}-${DOCKER_ARCH}" make config=${config} CC="$CC1" CXX="$CXX1" verbose=1 -j$(nproc) all
      docker run --rm -u pony:2000 -v $(pwd):/home/pony "ponylang/ponyc-ci:cross-llvm-${LLVM_VERSION}-${DOCKER_ARCH}" make config=${config} CC="$CC1" CXX="$CXX1" verbose=1 test-ci

      echo "Building and testing cross ponyc..."
      # build libponyrt for target arch
      docker run --rm -u pony:2000 -v $(pwd):/home/pony "ponylang/ponyc-ci:cross-llvm-${LLVM_VERSION}-${DOCKER_ARCH}" make config=${config} verbose=1 CC="${CROSS_CC}" CXX="${CROSS_CXX}" arch="${CROSS_ARCH}" tune="${CROSS_TUNE}" bits="${CROSS_BITS}" CFLAGS="${CROSS_CFLAGS}" CXXFLAGS="${CROSS_CXXFLAGS}" LDFLAGS="${CROSS_LDFLAGS}" -j$(nproc) libponyrt
      # build ponyc for target cross compilation
      docker run --rm -u pony:2000 -v $(pwd):/home/pony "ponylang/ponyc-ci:cross-llvm-${LLVM_VERSION}-${DOCKER_ARCH}" make config=${config} verbose=1 -j$(nproc) all PONYPATH=/usr/cross/lib cross_triple="${CROSS_TRIPLE}" cross_cpu="${CROSS_TUNE}" cross_arch="${CROSS_ARCH}" cross_linker="${CROSS_LINKER}"
      # run tests for cross built stdlib using ponyc cross building support
      docker run --rm -u pony:2000 -v $(pwd):/home/pony "ponylang/ponyc-ci:cross-llvm-${LLVM_VERSION}-${DOCKER_ARCH}" make config=${config} verbose=1 test-cross-ci PONYPATH=/usr/cross/lib cross_triple="${CROSS_TRIPLE}" cross_cpu="${CROSS_TUNE}" cross_arch="${CROSS_ARCH}" cross_linker="${CROSS_LINKER}" QEMU_RUNNER="${QEMU_RUNNER:-}"

      set +x
    fi

    # when RELEASE_CONFIG stops matching part of this case, move this logic
    if [[ "$TRAVIS_BRANCH" == "release" && "$TRAVIS_PULL_REQUEST" == "false" && "$RELEASE_DEBS" == "" ]]
    then
      download_llvm
      download_pcre
      set_linux_compiler
      build_appimage "$(cat VERSION)"
      ponyc-kickoff-copr
      ponyc-build-packages
      ponyc-build-docs
      ponyc-upload-docs
    fi
  ;;

  "osx")
    # when running for a nightly cron job or manual api requested job to make sure packaging isn't broken
    if [[ "$TRAVIS_BRANCH" == "master" && ( "$TRAVIS_EVENT_TYPE" == "cron" || "$TRAVIS_EVENT_TYPE" == "api" ) ]]
    then
      brew install ponyc --HEAD
      brew uninstall ponyc
      exit
    fi

    if [[ "$TRAVIS_BRANCH" != "release" ]]
    then
      brew update
      brew install pcre2
      brew install libressl

      export PATH=llvmsym/:$PATH
      mkdir llvmsym

      # 3.9.x
      brew install llvm@3.9
      brew link --overwrite --force llvm@3.9
      ln -fs "$(which llvm-config)" llvmsym/llvm-config-3.9
      ln -fs "$(which clang++)" llvmsym/clang++-3.9

      export CC1=clang-3.9
      export CXX1=clang++-3.9
      echo "Running LLVM 3.9 config=debug build..."
      export config=debug
      osx-ponyc-test
      echo "Running LLVM 3.9 config=release build..."
      export config=release
      osx-ponyc-test

      make clean
      brew uninstall llvm@3.9

      # 5.0.x
      brew install llvm@5
      brew link --overwrite --force llvm@5
      ln -fs "$(which llvm-config)" llvmsym/llvm-config-5.0
      ln -fs "$(which clang++)" llvmsym/clang++-5.0

      export CC1=clang-5.0
      export CXX1=clang++-5.0
      echo "Running LLVM 5.0 config=release build..."
      export config=release
      osx-ponyc-test

      make clean
      brew uninstall llvm@5

      # 6.0.x
      brew install llvm@6
      brew link --overwrite --force llvm@6
      ln -fs "$(which llvm-config)" llvmsym/llvm-config-6.0
      ln -fs "$(which clang++)" llvmsym/clang++-6.0

      export CC1=clang-6.0
      export CXX1=clang++-6.0
      echo "Running LLVM 6.0 config=release build..."
      export config=release
      osx-ponyc-test

      make clean
      brew uninstall llvm@6
    fi
  ;;

  *)
    echo "ERROR: An unrecognized OS. Consider OS: ${TRAVIS_OS_NAME}."
    exit 1
  ;;

esac
