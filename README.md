# Getting help

Need help? Not to worry, we have you covered.

We have a couple resources designed to help you learn, we suggest starting with the tutorial and from there, moving on to the Pony Patterns book. Additionally, standard library documentation is available online.

* [Tutorial](http://tutorial.ponylang.org).
* [Pony Patterns](http://patterns.ponylang.org) cookbook is in progress
* [Standard library docs](http://stdlib.ponylang.org/).

If you are looking for an answer "right now", we suggest you give our IRC channel a try. It's #ponylang on Freenode. If you ask a question, be sure to hang around until you get an answer. If you don't get one, or IRC isn't your thing, we have a friendly mailing list you can try. Whatever your question is, it isn't dumb, and we won't get annoyed.

* [Mailing list](mailto:pony+user@groups.io).
* [IRC](https://webchat.freenode.net/?channels=%23ponylang).

Think you've found a bug? Check your understanding first by writing the mailing list. Once you know it's a bug, open an issue.
* [Open an issue](https://github.com/ponylang/ponyc/issues)

# Trying it online

If you want a quick way to test or run code, checkout the [Playground](https://playground.ponylang.org/).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/ponylang/VS-pony)
* Visual Studio Code: [vscode-pony](https://marketplace.visualstudio.com/items?itemName=npruehs.pony)
* Vim:
    - [vim-pony](https://github.com/jakwings/vim-pony)
    - [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
    - [currycomb: Syntastic support](https://github.com/killerswan/pony-currycomb.vim)
    - [SpaceVim](http://spacevim.org), available as layer for Vim and [Neovim](https://neovim.io). Just follow [installation instructions](https://github.com/SpaceVim/SpaceVim) then put `call SpaceVim#layers#load('lang#pony')`inside configuration file (*$HOME/.SpaceVim.d/init.vim*)
* Emacs:
    - [ponylang-mode](https://github.com/seantallen/ponylang-mode)
    - [flycheck-pony](https://github.com/rmloveland/flycheck-pony)
    - [pony-snippets](https://github.com/SeanTAllen/pony-snippets)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)
* Micro: [micro-pony-plugin](https://github.com/Theodus/micro-pony-plugin)

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

If you're unfamiliar with Docker, remember to ensure that whatever path you provide for `/path/to/my-code` is a full path name and not a relative path, and also note the lack of a closing slash, `/`, at the *end* of the path name.

Note that if your host doesn't match the docker container, you'll probably have to run the resulting program inside the docker container as well, using a command like this:

```bash
docker run -v /path/to/my-code:/src/main ponylang/ponyc ./main
```

If you're using `docker-machine` instead of native docker, make sure you aren't using [an incompatible version of Virtualbox](#virtualbox).

### Docker for Windows

Pull the latest image as above.

```bash
docker pull ponylang/ponyc:latest
```

Share a local drive (volume), such as `c:`, with Docker for Windows, so that they are available to your containers. (Refer to [shared drives](https://docs.docker.com/docker-for-windows/#shared-drives) in the Docker for Windows documentation for details.)

Then you'll be able to run `ponyc` to compile a Pony program in a given directory, running a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc
```

Note the inserted drive letter. Replace with your drive letter as appropriate.

To run a program, run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc ./main
```

To compile and run in one step run a command like this:

```bash
docker run -v c:/path/to/my-code:/src/main ponylang/ponyc sh -c "ponyc && ./main"
```

### Docker and AVX2 Support

By default, the Pony Docker image is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

## Linux using an RPM package (via Bintray)

For Red Hat, CentOS, Oracle Linux, or Fedora Linux, the `release` builds are packaged and available on Bintray ([pony-language/ponyc-rpm](https://bintray.com/pony-language/ponyc-rpm)).

To install builds via DNF:
```bash
wget https://bintray.com/pony-language/ponyc-rpm/rpm -O bintray-pony-language-ponyc-rpm.repo
sudo mv bintray-pony-language-ponyc-rpm.repo /etc/yum.repos.d/

sudo dnf --refresh install ponyc
```

Or via Yum:
```bash
wget https://bintray.com/pony-language/ponyc-rpm/rpm -O bintray-pony-language-ponyc-rpm.repo
sudo mv bintray-pony-language-ponyc-rpm.repo /etc/yum.repos.d/

sudo yum install ponyc
```

Or via Zypper:
```bash
sudo zypper install gcc6 binutils-gold \
  zlib-devel libopenssl-devel pcre2-devel
wget https://bintray.com/pony-language/ponyc-rpm/rpm -O bintray-pony-language-ponyc-rpm.repo
sudo mv bintray-pony-language-ponyc-rpm.repo /etc/zypp/repos.d/

sudo zypper install ponyc
```

### RPM and AVX2 Support

By default, the Pony RPM package is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

## Linux using a DEB package (via Bintray)

For Ubuntu or Debian Linux, the `release` builds are packaged and available on Bintray ([pony-language/ponyc-debian](https://bintray.com/pony-language/ponyc-debian)).

To install builds via Apt (and install Bintray's public key):

```bash
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys "D401AB61 DBE1D0A2"
echo "deb https://dl.bintray.com/pony-language/ponyc-debian pony-language main" | sudo tee -a /etc/apt/sources.list
sudo apt-get update
sudo apt-get -V install ponyc
```

You may need to install `ponyc` (current) instead of `ponyc-release` (deprecated).  And if you have issues with package authentication you may need to comment out the `deb` line in `/etc/apt/sources.list`, do an update, and uncomment it again.  Feel free to ask for help if you have any problems!

### DEB and AVX2 Support

By default, the Pony DEB package is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

## Prepackaged Ubuntu Xenial

Make sure you have a gcc installed:

```bash
sudo apt-get install build-essential
```

Set the `CC` environment variable:

```bash
echo "export CC=`which gcc`" >> ~/.profile
export CC=`which gcc`
```

Install ponyc via bintray:

```bash
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys "D401AB61 DBE1D0A2"
echo "deb https://dl.bintray.com/pony-language/ponyc-debian pony-language main" | sudo tee -a /etc/apt/sources.list
sudo apt-get update
sudo apt-get -V install ponyc
```

## Arch Linux

Currently the ponyc package in Arch does not work because
Arch is using LLVM 5 and ponyc requires LLVM 3.7 to 3.9.

There is experimental support for building from source with LLVM 5.0.0,
but this may cause decreased performance or crashes in generated
applications.

Using [Docker](#using-docker) is one choice, another is to
use [ponyc-rpm](https://aur.archlinux.org/packages/ponyc-rpm/)

### ponyc-rpm
#### Prerequisites: `git` and `rpmextract`
```
sudo pacman -Syu git rpmextract
```
#### Instructions:
Clone the repo, change directory to the repo, run `makepkg -si`
or use your favorite AUR package manager.
```
git clone https://aur.archlinux.org/ponyc-rpm.git
cd ponyc-rpm
makepkg -si
```

#### Ponyc Usage
You must pass the `--pic` parameter to ponyc on Arch Linux
```
ponyc --pic
```

## Gentoo Linux

```bash
layman -a stefantalpalaru
emerge dev-lang/pony
```

A live ebuild is also available in the [overlay](https://github.com/stefantalpalaru/gentoo-overlay) (dev-lang/pony-9999) and for Vim users there's app-vim/pony-syntax.

## Linux using [Linuxbrew](http://linuxbrew.sh)

```bash
brew update
brew install ponyc
```

## NixOS Linux or any OS using [nix](http://nixos.org/nix/)

```bash
nix-env -i ponyc
```

## "cannot find 'ld'" error

If you get an error when trying to use `ponyc` to compile pony source
that looks like this:

```bash
collect2: fatal error: cannot find 'ld'
```

you might have to install the `ld-gold` linker. It can typically be
found by searching your distro's package repository for `binutils-gold`
or just `ld-gold`.


## Mac OS X using [Homebrew](http://brew.sh)

```bash
brew update
brew install ponyc
```

## Windows using ZIP (via Bintray)

Windows users will need to install:

- Visual Studio 2017 or 2015 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2017 or 2015 (available [here](http://landinghub.visualstudio.com/visual-cpp-build-tools)), and
  - If using Visual Studio 2015, install the Windows 10 SDK (available [here](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)).
  - If using Visual Studio 2017, install the `Windows 10 SDK (10.0.15063.0) for Desktop` from the Visual Studio installer.

Once you have installed the prerequisites, you can download the latest ponyc release from [bintray](https://dl.bintray.com/pony-language/ponyc-win/).

# Building ponyc from source

First of all, you need a compiler with decent C11 support. The following compilers are supported, though we recommend to use the most recent versions.

- GCC >= 4.7
- Clang >= 3.4
- MSVC >= 2015
- XCode Clang >= 6.0

Pony requires one of the following versions of LLVM:

- 3.7.1
- 3.8.1
- 3.9.1

There is experimental support for building with LLVM 4.0.1 or 5.0.0,
but this may result in decreased performance or crashes in generated
applications.

Compiling Pony is only possible on x86 and ARM (either 32 or 64 bits).

## Building on Linux

Get Pony-Sources from Github (More Information about Set Up Git https://help.github.com/articles/set-up-git/ ):
```bash
sudo apt install git
git clone git://github.com/ponylang/ponyc
```

[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

### Arch

```
pacman -S llvm make ncurses openssl pcre2 zlib
```

To build ponyc and compile helloworld:

```bash
make
./build/release/ponyc examples/helloworld
```

If you get errors like

```bash
/usr/bin/ld.gold: error: ./fb.o: requires dynamic R_X86_64_32 reloc against
 'Array_String_val_Trace' which may overflow at runtime; recompile with -fPIC
```

You need to rebuild `ponyc` with `default_pic=true`

```bash
make clean
make default_pic=true
./build/release/ponyc examples/helloworld
```

### Debian Jessie

Add the following to `/etc/apt/sources`:

```
deb http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.9 main
deb-src http://llvm.org/apt/jessie/ llvm-toolchain-jessie-3.9 main
```

Install the LLVM toolchain public GPG key, update `apt` and install packages:

```bash
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get update
sudo apt-get install make gcc g++ git zlib1g-dev libncurses5-dev \
                       libssl-dev llvm-3.9-dev
```

Debian Jessie and some other Linux distributions don't include pcre2 in their package manager. pcre2 is used by the Pony regex package. To download and build pcre2 from source:

```bash
wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
tar xvf pcre2-10.21.tar.bz2
cd pcre2-10.21
./configure --prefix=/usr
make
sudo make install
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Debian Sid

Install pony dependencies:

```bash
sudo apt-get update
sudo apt-get install make gcc g++ git zlib1g-dev libncurses5-dev \
  libssl-dev llvm llvm-dev libpcre2-dev
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make default_pic=true
./build/release/ponyc examples/helloworld
./helloworld
```

### Ubuntu Trusty

Add the LLVM apt report to /etc/apt/sources.list. Open `/etc/apt/sources.list` and add the following lines to the end of the file:

```
deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-3.9 main
deb-src http://apt.llvm.org/trusty/ llvm-toolchain-trusty-3.9 main
```

Add the LLVM repo as a trusted source:

```bash
cd /tmp
wget -O llvm-snapshot.gpg.key http://apt.llvm.org/llvm-snapshot.gpg.key
sudo apt-key add llvm-snapshot.gpg.key
```

Install dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential git zlib1g-dev libncurses5-dev \
  libssl-dev llvm-3.9
```

Install libprce2:

```bash
cd /tmp
wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
tar xjvf pcre2-10.21.tar.bz2
cd pcre2-10.21
./configure --prefix=/usr
make
sudo make install
```

Clone the ponyc repo:

```bash
cd ~/
git clone https://github.com/ponylang/ponyc.git
```

Build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Ubuntu Xenial

Add the LLVM apt repos to /etc/apt/sources.list. Open `/etc/apt/sources.list` and add the following lines to the end of the file:

```
deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-3.9 main
deb-src http://apt.llvm.org/xenial/ llvm-toolchain-xenial-3.9 main
```

Add the LLVM repo as a trusted source:

```bash
cd /tmp
wget -O llvm-snapshot.gpg.key http://apt.llvm.org/llvm-snapshot.gpg.key
sudo apt-key add llvm-snapshot.gpg.key
```

```bash
sudo apt-get update
sudo apt-get install -y build-essential git zlib1g-dev libncurses5-dev libssl-dev libpcre2-dev llvm-3.9
```

Clone the ponyc repo:

```bash
cd ~/
git clone https://github.com/ponylang/ponyc.git
```

Build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Fedora (25)

```bash
dnf check-update
sudo dnf install git gcc-c++ make openssl-devel pcre2-devel zlib-devel \
  llvm-devel ncurses-devel
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### OpenSUSE (Leap 24.3)

```bash
sudo zypper addrepo http://download.opensuse.org/repositories/devel:tools:compiler/openSUSE_Leap_42.3/devel:tools:compiler.repo
sudo zypper refresh
sudo zypper update
sudo zypper install git gcc-c++ make libopenssl-devel pcre2-devel zlib-devel \
  llvm3_9-devel binutils-gold
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Alpine (Edge)

Install build tools/dependencies:

```bash
apk add --update alpine-sdk libressl-dev binutils-gold llvm3.9 llvm3.9-dev \
  pcre2-dev libunwind-dev coreutils
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make default_pic=true
./build/release/ponyc examples/helloworld
./helloworld
```

### Other Linux distributions

You need to have the development versions of the following installed:

* 3.7.1, 3.8.1, or 3.9.1
* zlib
* ncurses
* pcre2
* libssl

There is experimental support for LLVM 4.0.1 and 5.0.0, but this may
result in decreased performance or crashes in generated applications.

If your distribution doesn't have a package for prce2, you will need to download and build it from source:

```bash
wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre2-10.21.tar.bz2
tar xvf pcre2-10.21.tar.bz2
cd pcre2-10.21
./configure --prefix=/usr
make
sudo make install
```

Finally to build ponyc, compile and run the hello world app:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

## Building on DragonFly

Dragonfly has been tested on 64-bit X86 DragonFly 4.8.

First, install the required dependencies:

```bash
sudo pkg install gmake
sudo pkg install llvm38
sudo pkg install pcre2
```

This will build ponyc and compile helloworld:

```bash
gmake
./build/release/ponyc examples/helloworld
```

## Building on FreeBSD

First, install the required dependencies:

* FreeBSD 11 for amd64 (64-bit).  It is extremely difficult to
coordinate the LLVM version, operating system support for atomics, and
Pony to work in harmony on FreeBSD 10 or earlier.  (See additional
info below that mentions problems with 32-bit executable support.)

* The following packages, with their installation commands:

```bash
sudo pkg install gmake
sudo pkg install llvm38
sudo pkg install pcre2
sudo pkg install libunwind
```

This will build ponyc and compile helloworld:

```bash
gmake
./build/release/ponyc examples/helloworld
```

## Building on Mac OS X
[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

You'll need llvm 3.7.1, 3.8.1, or 3.9.1 and the pcre2 library to build Pony. You can use either homebrew or MacPorts to install your dependencies.

There is experimental support for LLVM 4.0.1 or 5.0.0, but this may result in
decreased performance or crashes in generated applications.

Installation via [homebrew](http://brew.sh):
```
brew update
brew install llvm@3.9 pcre2 libressl
```

Installation via [MacPorts](https://www.macports.org):
```
sudo port install llvm-3.9 pcre2 libressl
sudo port select --set llvm mp-llvm-3.9
```

Launch the build with `make` after installing the dependencies:
```
make
./build/release/ponyc examples/helloworld
```

## Building on Windows
[![Windows](https://ci.appveyor.com/api/projects/status/kckam0f1a1o0ly2j?svg=true)](https://ci.appveyor.com/project/sylvanc/ponyc)

_Note: it may also be possible (as tested on build 14372.0 of Windows 10) to build Pony using the [Ubuntu 14.04](#ubuntu-1404-1510-1604) instructions inside [Bash on Ubuntu on Windows](https://msdn.microsoft.com/en-us/commandline/wsl/about)._

Building on Windows requires the following:

- Visual Studio 2017 or 2015 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2017 or 2015 (available [here](http://landinghub.visualstudio.com/visual-cpp-build-tools)), and
  - If using Visual Studio 2015, install the Windows 10 SDK (available [here](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)).
  - If using Visual Studio 2017, install the `Windows 10 SDK (10.0.15063.0) for Desktop` from the Visual Studio installer.
- [Python](https://www.python.org/downloads) (3.6 or 2.7) needs to be in your PATH.

In a command prompt in the `ponyc` source directory, run the following:

```
make.bat configure
```

(You only need to run this the first time you build the project.)

```
make.bat build test
```

This will automatically perform the following steps:

- Download some pre-built libraries used for building the Pony compiler and standard library.
  - [LLVM](http://llvm.org)
  - [LibreSSL](https://www.libressl.org/)
  - [PCRE](http://www.pcre.org)
- Build the pony compiler in the `build-<config>-<llvm-version>` directory.
- Build the unit tests for the compiler and the standard library.
- Run the unit tests.

You can provide the following options to `make.bat` when running the `build` or `test` commands:

- `--config debug|release`: whether or not to build a debug or release build (`release` is the default).
- `--llvm <version>`: the LLVM version to build against (`3.9.1` is the default).

Note that you need to provide these options each time you run make.bat; the system will not remember your last choice.

Other commands include `clean`, which will clean a specified configuration; and `distclean`, which will wipe out the entire build directory.  You will need to run `make configure` after a distclean.

## Building with link-time optimisation (LTO)

Link-time optimizations provide a performance improvement. You should strongly consider turning on LTO if you build ponyc from source. It's off by default as it comes with some caveats:

- If you aren't using clang as your linker, we've seen LTO generate incorrect binaries. It's rare but it can happen. Before turning on LTO you need to be aware that it's possible.

- If you are on MacOS, turning on LTO means that if you upgrade your version of XCode, you will have to rebuild your Pony compiler. You won't be able to link Pony programs if there is a mismatch between the version of XCode used to build the Pony runtime and the version of XCode you currently have installed.

You can enable LTO when building the compiler in release mode. There are slight differences between platforms so you'll need to do a manual setup. LTO is enabled by setting `lto` to `yes` in the build command line:

```bash
make lto=yes
```

If the build fails, you have to specify the LTO plugin for your compiler in the `LTO_PLUGIN` variable. For example:

```bash
make LTO_PLUGIN=/usr/lib/LLVMgold.so
```

Refer to your compiler documentation for the plugin to use in your case.

## Building the runtime as an LLVM bitcode file

If you're compiling with Clang, you can build the Pony runtime as an LLVM bitcode file by setting `runtime-bitcode` to `yes` in the build command line:

```bash
make runtime-bitcode=yes
```

Then, you can pass the `--runtimebc` option to ponyc in order to use the bitcode file instead of the static library to link in the runtime:

```bash
ponyc --runtimebc
```

This functionnality boils down to "super LTO" for the runtime. The Pony compiler will have full knowledge of the runtime and will perform advanced interprocedural optimisations between your Pony code and the runtime. If your're looking for maximum performance, you should consider this option. Note that this can result in very long optimisation times.

## VirtualBox

Pony binaries can trigger illegal instruction errors under VirtualBox 4.x, for at least the x86_64 platform and possibly others.

Use VirtualBox 5.x to avoid possible problems.


You can learn more about [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions#Advanced_Vector_Extensions_2) support.

## Building Pony on Non-x86 platforms

On ARM platforms, the default gcc architecture specification used in the Makefile of _native_ does not work correctly, and can even result in the gcc compiler crashing.  You will have to override the compiler architecture specification on the _make_ command line.  For example, on a RaspberryPi2 you would say:
```bash
make arch=armv7
```

To get a complete list of acceptable architecture names, use the gcc command:

```bash
gcc -march=none
```

This will result in an error message plus a listing off all architecture types acceptable on your platform.
