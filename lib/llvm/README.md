# Build llvm, binaryen and wabt for wasm
Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

## Prerequesites
  * Gcc 6+
  * ninja

## Building
Options:
- BUILD_ENGINE=<Ninja|"Unix Makefiles"> the type of build to create (default Ninja)
- BUILD_TYPE=<Release|Debug> the type of build to create (default Release)
- CPUs=N where N is the number of cpus passed to -j option (default 3)
- INSTALL_DIR=xxx where xxx is the path to the install directory (default ./dist)
```
make build BUILD_ENGINE=Ninja CPUS=4
```
## Install
You must use the same BUILD_ENGINE and INSTALL_DIR as used when building or none if not specified when building
```
make install BUILD_ENGINE=Ninja
```
## Using the results
On my Arch Linux system the following compiles ponyc
```
$ CC=clang CXX=clang++ PATH=./lib/llvm/dist/bin:$PATH make default_pic=true default_ssl=openssl_1.1.0
```
And here is the resulting version string:
```
$ ./build/release/ponyc --version
0.21.3-02f6db76 [release]
compiled with: llvm 6.0.0 -- clang version 6.0.0 (tags/RELEASE_600/final 330495)
Defaults: pic=true ssl=openssl_1.1.0
```
## Clean
Clean binaries and install targets
```
make clean
```

## Distributation clean
Clean binaries, install targets and sources
```
make dist-clean
```
