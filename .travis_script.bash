#! /bin/bash

set -o errexit
set -o nounset

# include install commands
. .travis_install.bash
# include various build commands
. .travis_commands.bash

case "${TRAVIS_OS_NAME}" in
  "linux")
    # when RELEASE_CONFIG stops matching part of this case, move this logic
    if [[ "$TRAVIS_BRANCH" == "release" && "$TRAVIS_PULL_REQUEST" == "false" ]]
    then
      download_llvm
      download_pcre
      set_linux_compiler
      ponyc-build-packages
      ponyc-build-docs
    fi
  ;;

  "osx")
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
      ponyc-test
      echo "Running LLVM 3.9 config=release build..."
      export config=release
      ponyc-test

      make clean
      brew uninstall llvm@3.9

      # 4.0.x
      brew install llvm@4
      brew link --overwrite --force llvm@4
      ln -fs "$(which llvm-config)" llvmsym/llvm-config-4.0
      ln -fs "$(which clang++)" llvmsym/clang++-4.0

      export CC1=clang-4.0
      export CXX1=clang++-4.0
      #echo "Running LLVM 4.0 config=debug build..."
      #export config=debug
      #ponyc-test
      echo "Running LLVM 4.0 config=release build..."
      export config=release
      ponyc-test

      make clean
      brew uninstall llvm@4

      # 5.0.x
      brew install llvm@5
      brew link --overwrite --force llvm@5
      ln -fs "$(which llvm-config)" llvmsym/llvm-config-5.0
      ln -fs "$(which clang++)" llvmsym/clang++-5.0

      export CC1=clang-5.0
      export CXX1=clang++-5.0
      #echo "Running LLVM 5.0 config=debug build..."
      #export config=debug
      #ponyc-test
      echo "Running LLVM 5.0 config=release build..."
      export config=release
      ponyc-test

      make clean
      brew uninstall llvm@5
    fi
  ;;

  *)
    echo "ERROR: An unrecognized OS. Consider OS: ${TRAVIS_OS_NAME}."
    exit 1
  ;;

esac
