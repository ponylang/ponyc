# Getting help

* [Open an issue!](https://github.com/ponylang/ponyc/issues)
* Use the [mailing list](mailto:pony+user@groups.io).
* Join [the `#ponylang` IRC channel on freenode](https://webchat.freenode.net/?channels=%23ponylang).
* A tutorial is available [here](http://tutorial.ponylang.org).
* A [cookbook style book of patterns](http://patterns.ponylang.org) is in progress
* Standard library docs are available [here](http://ponylang.github.io/ponyc/).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/ponylang/VS-pony)
* Vim: [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
* Emacs:
    - [ponylang-mode](https://github.com/seantallen/ponylang-mode)
    - [flycheck-pony](https://github.com/rmloveland/flycheck-pony)
    - [pony-snippets](https://github.com/SeanTAllen/pony-snippets)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)

# Installation

## Using Docker

Want to use the latest revision of Pony source, but don't want to build from source yourself? You can run the `ponylang/ponyc` Docker container, which is created from an automated build at each commit to master.

You'll need to install Docker using [the instructions here](https://docs.docker.com/engine/installation/). Then you can pull the latest `ponylang/ponyc` image using this command:

```bash
docker pull ponylang/ponyc:latest
```

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc
```

Note that if your host doesn't match the docker container, you'll probably have to run the resulting program inside the docker container as well, using a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc ./main
```

## Mac OS X using [Homebrew](http://brew.sh)

The homebrew version is currently woefully out of date. We are transitioning to
a new release system that will keep homebrew up to date. For now, please build
from source.

```bash
$ brew update
$ brew install ponyc
```

## Linux

### Gentoo

```bash
layman -a stefantalpalaru
emerge dev-lang/pony
```

A live ebuild is also available in the
[overlay](https://github.com/stefantalpalaru/gentoo-overlay)
(dev-lang/pony-9999) and for Vim users there's app-vim/pony-syntax.

### Other distributions

We're transitioning to a new binary release system. For now, please build from source.

## Windows

You will need to build from source.

# Building ponyc from source

Pony requires LLVM 3.6, 3.7 or 3.8. Please note that **LLVM 3.7.0 does not work**. If you are using LLVM 3.7.x, you need to use 3.7.1. If you are using LLVM 3.6.x, make sure to use 3.6.2.

## Building on Linux
[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

First, install LLVM using your package manager. You may need to install zlib, ncurses, pcre2, and ssl as well. Instructions for some specific distributions follow.

APT packages for LLVM 3.7 and 3.8, for Debian and Ubuntu, are provided by the LLVM build server.

Please follow the instructions at http://llvm.org/apt/ for installing the GPG keys and packages for your distribution.

#### Notes:

On Ubuntu,
`apt-get install llvm-dev` installs LLVM-3.6.

When installing the full set of LLVM packages, the ```apt-get install``` instructions include the ```clang-modernize``` package. This should be changed to ```clang-tidy``` due to a recent name change.


### Debian Jessie

Add the following to `/etc/apt/sources`:

```
deb http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main
deb-src http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main
```

Install the LLVM toolchain public GPG key, update `apt` and install packages:

```bash
$ wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
$ sudo apt-get update
$ sudo apt-get install make gcc g++ git zlib1g-dev libncurses5-dev \
                       libssl-dev llvm-3.8-dev
```

Debian Jessie and some other Linux distributions don't include pcre2 in their package manager. pcre2 is used by the Pony regex package. To download and build pcre2 from source:

```bash
$ wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
$ tar xvf pcre2-10.21.tar.bz2
$ cd pcre2-10.21
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

To build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

### Ubuntu (12.04, 14.04, 15.10, 16.04)

```bash
$ sudo apt-get install build-essential git \
                       zlib1g-dev libncurses5-dev libssl-dev
```

Ubuntu and some other Linux distributions don't include pcre2 in their package manager. pcre2 is used by the Pony regex package. To download and build pcre2 from source:

```bash
$ wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
$ tar xvf pcre2-10.21.tar.bz2
$ cd pcre2-10.21
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

To build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on FreeBSD

First, install the required dependencies:

```bash
sudo pkg install gmake
sudo pkg install llvm38
sudo pkg install pcre2
sudo pkg install libunwind
```

This will build ponyc and compile helloworld:

```bash
$ gmake config=release
$ ./build/release/ponyc examples/helloworld
```

Please note that on 32-bit X86, using LLVM 3.7 or 3.8 on FreeBSD currently produces executables that don't run. Please use LLVM 3.6. 64-bit X86 does not have this problem, and works fine with LLVM 3.7 and 3.8.

## Building on Mac OS X
[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

You'll need llvm 3.6.2, 3.7.1, or 3.8 and the pcre2 library to build Pony.

Either install them via [homebrew](http://brew.sh):
```
$ brew update
$ brew install homebrew/versions/llvm38 pcre2 libressl
```

Or install them via macport:
```
$ sudo port install llvm-3.8 pcre2 libressl
$ sudo port select --set llvm mp-llvm-3.8
```

Then launch the build with Make:
```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on Windows
[![Windows](https://ci.appveyor.com/api/projects/status/kckam0f1a1o0ly2j?svg=true)](https://ci.appveyor.com/project/sylvanc/ponyc)

The LLVM prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.7 or 3.8 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

LLVM recommends using the GnuWin32 unix tools; your mileage may vary using MSYS or Cygwin.

- Install GnuWin32 using the [GetGnuWin32](http://getgnuwin32.sourceforge.net/) tool.
- Install [Python](https://www.python.org/downloads/release/python-351/) (3.5 or 2.7).
- Install [CMake](https://cmake.org/download/).
- Get the LLVM source (e.g. 3.7.1 is at [3.7.1](http://llvm.org/releases/3.7.1/llvm-3.7.1.src.tar.xz)).
- Make sure you have VS2015 with the C++ tools installed.
- Generate LLVM VS2015 configuration with CMake. You can use the GUI to configure and generate the VS projects; make sure you use the 64-bit generator (**Visual Studio 14 2015 Win64**), and set the `CMAKE_INSTALL_PREFIX` to where you want LLVM to live.
- Open the LLVM.sln in Visual Studio 2015 and build the INSTALL project in the LLVM solution in Release mode.

Building Pony requires [Premake 5](https://premake.github.io).

- Get the [PreMake 5](https://premake.github.io/download.html#v5) executable.
- Get the [PonyC source](https://github.com/ponylang/ponyc).
- Run `premake5.exe --with-tests --to=..\vs vs2015` to generate the PonyC
  solution.
- Build ponyc.sln in Release mode.

In order to run the pony compiler, you'll need a few libraries in your environment (pcre2, libssl, libcrypto).

There is a third-party utility that will get the libraries and set up your environment:

- Install [7-Zip](http://www.7-zip.org/a/7z1514-x64.exe), make sure it's in your PATH.
- Open a **VS2015 x64 Native Tools Command Prompt** (things will not work correctly otherwise!) and run:

```
> git clone git@github.com:kulibali/ponyc-windows-libs.git
> cd ponyc-windows-libs
> .\getlibs.bat
> .\setenv.bat
```

Now you can run the pony compiler and tests:

```
> cd path_to_pony_source
> build\release\testc.exe
> build\release\testrt.exe
> build\release\ponyc.exe -d -s packages\stdlib
> .\stdlib
```

## Building with link-time optimisation (LTO)

You can enable LTO when building the compiler in release mode. There are slight differences between platforms so you'll need to do a manual setup. LTO is enabled by setting `lto` to `yes` in the build command line:

```bash
$ make config=release lto=yes
```

If the build fails, you have to specify the LTO plugin for your compiler in the `LTO_PLUGIN` variable. For example:

```bash
$ make config=release LTO_PLUGIN=/usr/lib/LLVMgold.so
```

Refer to your compiler documentation for the plugin to use in your case.

LTO is enabled by default on OSX.


## VirtualBox

Pony binaries can trigger illegal instruction errors under VirtualBox 4.x, for at least the x86_64 platform and possibly others.

Use VirtualBox 5.x to avoid possible problems.
