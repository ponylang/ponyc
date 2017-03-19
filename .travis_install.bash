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

download-llvm(){
  # Given the Travis LLVM version envvar,
  # download an LLVM binary and install it.

  # note 3.6.2 was different:
  #wget "http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-ubuntu-14.04.tar.xz"

  wget "http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8.tar.xz"
  tar -xvf clang+llvm*
  cd clang+llvm* && sudo mkdir /tmp/llvm && sudo cp -r ./* /tmp/llvm/
  sudo ln -s "/tmp/llvm/bin/llvm-config" "/usr/local/bin/${LLVM_CONFIG}"
}

download-pcre(){
  # Download, build, and install PCRE 2.
  wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
  tar -xjvf pcre2-10.21.tar.bz2
  cd pcre2-10.21 && ./configure --prefix=/usr && make && sudo make install
}

case "${TRAVIS_OS_NAME}:${LLVM_CONFIG}" in

  "linux:llvm-config-3.7")
    download-llvm
    download-pcre
  ;;

  "linux:llvm-config-3.8")
    download-llvm
    download-pcre
  ;;

  "linux:llvm-config-3.9")
    download-llvm
    download-pcre
  ;;

  "osx:llvm-config-3.7")
    brew update
    #brew install gmp
    #brew link --overwrite gmp
    brew install pcre2
    brew install libressl

    brew install llvm37
  ;;

  "osx:llvm-config-3.8")
    brew update
    #brew install gmp
    #brew link --overwrite gmp
    brew install pcre2
    brew install libressl

    brew install llvm38
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

    # do this elsewhere:
    #export PATH=llvmsym/:$PATH
  ;;

  *)
    echo "ERROR: An unrecognized OS and LLVM tuple was found! Consider OS: ${TRAVIS_OS_NAME} and LLVM: ${LLVM_CONFIG}"
    exit 1
  ;;

esac
