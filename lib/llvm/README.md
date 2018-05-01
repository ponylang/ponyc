# Build llvm and optionally clang plus compiler-rt
Based on [yurydelendik wasmllvm](https://gist.github.com/yurydelendik/4eeff8248aeb14ce763e)

## Prerequesites
  * Gcc 6+
  * ninja
  * gold

## Building
Options:
- BUILD_ENGINE=<Ninja|"Unix Makefiles"> the type of build to create (default Ninja)
- BUILD_TYPE=<Release|Debug> the type of build to create (default Release)
- CPUs=N where N is the number of cpus passed to -j option (default 1)
- INSTALL_DIR=xxx where xxx is the path to the install directory (default ./dist)
- LINKER=<gold|bfd> the linker to use (default gold)

Build just llvm
```
make build BUILD_ENGINE=Ninja CPUS=4
```
Build llvm and clang + compiler-rt
```
make all BUILD_ENGINE=Ninja
```
This has been tested on a 32G RAM desktop, 16G laptop and a 1G VM. Be careful
with the CPUS parameter, with the desktop CPUS=10 wasn't a problem but with
the laptop I used CPUS=2 and the tiny VM I used CPUS=1. YMMV.

## Install
You must use the same BUILD_ENGINE and INSTALL_DIR as used when building or none if not specified when building
```
make install BUILD_ENGINE=Ninja
``:`
## Using the results
The following or a variant can be use to compile ponyc
```
$ PATH=./lib/llvm/dist/bin:$PATH make default_pic=true default_ssl=openssl_1.1.0
```
or add CC/CXX to use the clang you may have built
```
$ CC=clang CXX=clang++ PATH=./lib/llvm/dist/bin:$PATH make default_pic=true default_ssl=openssl_1.1.0
```
And here is the resulting version string:
```
$ ./build/release/ponyc --version
0.21.3-02f6db76 [release]
compiled with: llvm 6.0.0 -- clang version 6.0.0 (tags/RELEASE_600/final)
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
