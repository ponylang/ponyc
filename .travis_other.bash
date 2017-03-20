#! /bin/bash

set +o errexit
set +o nounset
set +o

here="$(cd "$(dirname "${0}")" && pwd)"
source "${here}/.travis_common.bash"

case "${TRAVIS_OS_NAME}:${LLVM_CONFIG}" in

  "linux:llvm-config-3.7")
    download-llvm
    download-pcre

    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "linux:llvm-config-3.8")
    download-llvm
    download-pcre

    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "linux:llvm-config-3.9")
    download-llvm
    download-pcre

    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "osx:llvm-config-3.7")
    brew update
    #brew install gmp
    #brew link --overwrite gmp
    brew install pcre2
    brew install libressl

    brew install llvm37

    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "osx:llvm-config-3.8")
    brew update
    #brew install gmp
    #brew link --overwrite gmp
    brew install pcre2
    brew install libressl

    brew install llvm38

    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  "osx:llvm-config-3.9")
    brew update
    #brew install gmp
    #brew link --overwrite gmp
    brew install pcre2
    brew install libressl

    brew install llvm@3.9
    brew link --overwrite --force llvm@3.9
    mkdir llvmsym
    ln -s "$(which llvm-config)" llvmsym/llvm-config-3.9
    ln -s "$(which clang++)" llvmsym/clang++-3.9

    export PATH=llvmsym/:$PATH
    make CC="$CC1" CXX="$CXX1" test-ci
  ;;

  *)
    echo "ERROR: An unrecognized OS and LLVM tuple was found! Consider OS: ${TRAVIS_OS_NAME} and LLVM: ${LLVM_CONFIG}"
    exit 1
  ;;

esac
