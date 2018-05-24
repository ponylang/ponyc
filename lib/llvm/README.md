# Build llvm

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

Build llvm-3.9.1 using the defaults:
```
make
```
Example over riding the defaults for building llvm-6.0.0:
```
make llvm-6.0.0 -j12 BUILD_ENGINE=Ninja BULID_TYPE=Debug INSTALL_DIR=./bin LINKER=bfd
```
Building has been tested on a 32G RAM desktop, 16G laptop and a 1G VM. Be careful
using -j for parallel build with the desktop -j10 wasn't a problem but with
the laptop I needed to use -j2 and the tiny VM -j1. YMMV.

## Install
You must use the same BUILD_ENGINE and INSTALL_DIR as used when building or none if not specified when building
```
make install
```
## Clean
Clean binaries
```
make clean
```
