# Getting help 

* [Open an issue!](https://github.com/ponylang/ponyc/issues)
* Use the [mailing list](mailto:pony+user@groups.io).
* Join ```#ponylang``` on [freenode](http://freenode.net/irc_servers.shtml).
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

## Building on Linux 
[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

First, install LLVM 3.6.2, 3.7.1 or 3.8 using your package manager. You may 
need to install zlib, ncurses, pcre2, and ssl as well. Instructions for some
specific distributions follow.

### Debian Jessie

Add the following to `/etc/apt/sources`:

```
deb http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main
deb-src http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.8 main
```

Install the LLVM toolchain public GPG key, update `apt` and install
packages:

```bash
$ wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
$ sudo apt-get update
$ sudo apt-get install make gcc g++ git zlib1g-dev libncurses5-dev \
                       libssl-dev llvm-3.8-dev
```

Debian Jessie and some other Linux distributions don't include pcre2 in their
package manager. pcre2 is used by the Pony regex package. To download and
build pcre2 from source:

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

### Ubuntu 15.10

```bash
$ sudo apt-get install build-essential git llvm-dev \
                       zlib1g-dev libncurses5-dev libssl-dev
```

Ubuntu 15.10 and some other Linux distributions don't include pcre2 in their
package manager. pcre2 is used by the Pony regex package. To download and
build pcre2 from source:

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

Please note that the LLVM 3.8 apt packages do not include debug symbols. As a 
result, the `ponyc config=debug` build fails when using those packages. If you 
need a debug compiler built with LLVM 3.8, you will need to build LLVM from 
source.

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
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

Please note that on 32-bit X86, using LLVM 3.7 or 3.8 on FreeBSD currently 
produces executables that don't run. Please use LLVM 3.6. 64-bit X86 does not 
have this problem, and works fine with LLVM 3.7 and 3.8.

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

The LLVM prebuilt binaries for Windows do NOT include the LLVM development 
tools and libraries. Instead, you will have to build and install LLVM 3.7 or 
3.8 from source. You will need to make sure that the path to LLVM/bin (location 
of llvm-config) is in your PATH variable.

LLVM recommends using the GnuWin32 unix tools; your mileage may vary using 
MSYS or Cygwin.

- Install GnuWin32 using the [GetGnuWin32](http://getgnuwin32.sourceforge.net/) 
  tool.
- Install [Python](https://www.python.org/downloads/release/python-351/) (3.5 or 
  2.7).
- Install [CMake](https://cmake.org/download/).
- Get the LLVM source (e.g. 3.7.1 is 
  at [3.7.1](http://llvm.org/releases/3.7.1/llvm-3.7.1.src.tar.xz)).
- Make sure you have VS2015 with the C++ tools installed.
- Generate LLVM VS2015 configuration with CMake. You can use the GUI to 
  configure and generate the VS projects; make sure you use the 64-bit 
  generator (**Visual Studio 14 2015 Win64**), and set the 
  `CMAKE_INSTALL_PREFIX` to where you want LLVM to live.
- Open the LLVM.sln in Visual Studio 2015 and build the INSTALL project in 
  the LLVM solution in Release mode.

Building Pony requires [Premake 5](https://premake.github.io).

- Get the [PreMake 5](https://premake.github.io/download.html#v5) executable.
- Get the [PonyC source](https://github.com/ponylang/ponyc).
- Run `premake5.exe --with-tests --to=..\vs vs2015` to generate the PonyC
  solution.
- Change the **Character Set** property of each project in the PonyC solution
  to **Not Set**.
- Build ponyc.sln in Release mode.

In order to run the pony compiler, you'll need a few libraries in your 
environment (pcre2, libssl, libcrypto). 

There is a third-party utility that will get the libraries and set up your 
environment:

- Install [7-Zip](http://www.7-zip.org/a/7z1514-x64.exe), make sure it's in 
  your PATH.
- Open a **VS2015 x64 Native Tools Command Prompt** (things will not work 
  correctly otherwise!) and run:

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
