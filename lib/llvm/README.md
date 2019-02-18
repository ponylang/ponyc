# Build llvm sources

See [llvm getting started](http://llvm.org/docs/GettingStarted.html) for more
information on building llvm.

## Prerequesites
  * Gcc 6+
  * cmake
  * ninja &| make
  * gold &| bfd

## Building
Options:
- LLVM_BUILD_ENGINE=<Ninja|"Unix Makefiles"> the type of build to create (default "Unix Makefiles")
- LLVM_BUILD_TYPE=<Release|Debug|RelWithDebInfo|MinSizeRel> the type of build to create (default Release)
- LLVM_INSTALL_DIR=xxx where xxx is the path to the install directory (default ./dist)
- LLVM_PROJECT_LIST=<""|"all"|<comma seperated list of targets>>
    if "all" the list is: "clang;clang-tools-extra;libcxx;libcxxabi;libunwind;compiler-rt;lld;lldb;polly;debuginfo-tests"
- LINKER=<gold|bfd|ld.lld> the linker to use (default bfd)

Build using the defaults as defined by lib/llvm/src submodule:
```
make -j10
```
Example over riding the defaults for building llvm-7.0.0:
```
make llvm-7.x -j12 LLVM_BUILD_ENGINE=Ninja LLVM_BULID_TYPE=Debug LLVM_INSTALL_DIR=./bin LINKER=gold
```
Building has been tested on a 32G RAM desktop, 16G laptop and a 1G VM. Be careful
using -j for parallel build with the desktop -j10 wasn't a problem but with
the laptop I needed to use -j2 and the tiny VM -j1. YMMV.

## Install
You must use the same LLVM_BUILD_ENGINE and LLVM_INSTALL_DIR as used when building or none if not specified when building
```
make install
```
## Clean
Clean binaries
```
make clean
```
## Distributation Clean
Clean binaries and sources
```
make distclean
```
