# Build llvm and optionally clang plus compiler-rt
Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

## Prerequesites
  * Gcc 6+
  * cmake
  * ninja &| make
  * gold &| bfd

## Building
Options:
- BUILD_ENGINE=<Ninja|"Unix Makefiles"> the type of build to create (default "Unix Makefiles")
- BUILD_TYPE=<Release|Debug> the type of build to create (default Release)
- INSTALL_DIR=xxx where xxx is the path to the install directory (default ./dist)
- LINKER=<gold|bfd> the linker to use (default bfd)
- LLVM_PROJ=<tags/RELEASE_391/final|tags/RELEASE_600/final> (default RELEASE_391)

Build just llvm which using the defaults:
```
make
```
Build llvm and clang + compiler-rt using the defaults:
```
make all
```
Example over riding the defaults for building llvm:
```
make BUILD_ENGINE=Ninja BULID_TYPE=Debug INSTALL_DIR=./bin LINKER=bfd
```
Building has been tested on a 32G RAM desktop, 16G laptop and a 1G VM. Be careful
using -j for parallel build with the desktop -j10 wasn't a problem but with
the laptop I needed to use -j2 and the tiny VM -j1. YMMV.

## Install
You must use the same BUILD_ENGINE and INSTALL_DIR as used when building or none if not specified when building
```
make install
```
## Using the results
Add use=pony_lib_llvm when compiling ponyc
```
$ make default_pic=true default_ssl=openssl_1.1.0 use=pony_lib_llvm
```
And here is a resulting version string:
```
$ ./build/release/ponyc --version
0.21.3-9bd9e1e3 [release]
compiled with: llvm 3.9.1 -- cc (GCC) 7.3.1 20180406
Defaults: pic=true ssl=openssl_1.1.0
```
or add CC/CXX to use the clang you may have built
```
$ CC=clang CXX=clang++ make default_pic=true default_ssl=openssl_1.1.0 use=pony_lib_llvm
```
## Clean
Clean binaries
```
make clean
```

## Distributation clean
Clean binaries and sources
```
make distclean
```
