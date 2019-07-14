#! /bin/bash

set -o errexit
set -o nounset

download_llvm(){
  echo "Downloading and installing the LLVM specified by envvars..."

  git lfs clone -I "clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8.tar.xz" https://github.com/ponylang/ponyc-llvm-dependencies.git

  pushd ponyc-llvm-dependencies
  tar -xf "clang+llvm-${LLVM_VERSION}-x86_64-linux-gnu-debian8.tar.xz"
  pushd clang+llvm* && sudo mkdir /tmp/llvm && sudo cp -r ./* /tmp/llvm/
  sudo ln -s "/tmp/llvm/bin/llvm-config" "/usr/local/bin/${LLVM_CONFIG}"
  popd
  popd
}

set_linux_compiler(){
  echo "Setting $ICC1 and $ICXX1 as default compiler"

  sudo update-alternatives --install /usr/bin/gcc gcc "/usr/bin/$ICC1" 60 --slave /usr/bin/g++ g++ "/usr/bin/$ICXX1"
}
