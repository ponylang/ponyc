#! /bin/bash

set -o errexit
set -o nounset

if [[ "$TRAVIS_BRANCH" == "release" && "$FAVORITE_CONFIG" != "yes" ]]
then
  echo "This is a release branch and there's nothing this matrix element must do."
  exit 0
fi

download_llvm(){
  echo "Downloading and installing the LLVM specified by envvars..."

  wget "http://llvm.org/releases/${LLVM_VERSION}/clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8.tar.xz"
  tar -xvf clang+llvm*
  pushd clang+llvm* && sudo mkdir /tmp/llvm && sudo cp -r ./* /tmp/llvm/
  sudo ln -s "/tmp/llvm/bin/llvm-config" "/usr/local/bin/${LLVM_CONFIG}"
  popd
}

download_pcre(){
  echo "Downloading and building PCRE2..."

  wget "ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2"
  tar -xjvf pcre2-10.21.tar.bz2
  pushd pcre2-10.21 && ./configure --prefix=/usr && make && sudo make install
  popd
}

echo "Installing ponyc build dependencies..."

case "${TRAVIS_OS_NAME}:${LLVM_CONFIG}" in

  "linux:llvm-config-3.7")
    download_llvm
    download_pcre
  ;;

  "linux:llvm-config-3.8")
    download_llvm
    download_pcre
  ;;

  "linux:llvm-config-3.9")
    download_pcre

    echo "Logging LLVM binary locations (installed via APT)..."
    echo "llvm-config-3.9: $(which llvm-config-3.9)"
    echo "clang++-3.9: $(which clang++-3.9)"
    echo "clang-3.9: $(which clang-3.9)"

    found_version="$(llvm-config-3.9 --version)"
    echo "LLVM version: $found_version"

    if [[ "$found_version" != "$LLVM_VERSION" ]]
    then
      echo "WARNING: the LLVM version expected ($LLVM_VERSION) does not match that installed ($found_version)!"
    fi
  ;;

  "osx:llvm-config-3.7")
    brew update
    brew install pcre2
    brew install libressl

    brew install llvm37
  ;;

  "osx:llvm-config-3.8")
    brew update
    brew install pcre2
    brew install libressl

    brew install llvm38
  ;;

  "osx:llvm-config-3.9")
    brew update
    brew install shellcheck
    shellcheck ./.*.bash ./*.bash

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
