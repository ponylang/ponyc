[![Backers on Open Collective](https://opencollective.com/ponyc/backers/badge.svg)](#backers)
 [![Sponsors on Open Collective](https://opencollective.com/ponyc/sponsors/badge.svg)](#sponsors)

# Contributors

This project exists thanks to all the people who contribute. [[Contribute](CONTRIBUTING.md)].
<a href="https://github.com/ponylang/ponyc/graphs/contributors"><img src="https://opencollective.com/ponyc/contributors.svg?width=890&button=false" /></a>


# Backers

Thank you to all our backers! 🙏 [[Become a backer](https://opencollective.com/ponyc#backer)]

<a href="https://opencollective.com/ponyc#backers" target="_blank"><img src="https://opencollective.com/ponyc/backers.svg?width=890"></a>


# Sponsors

Support this project by becoming a sponsor. Your logo will show up here with a link to your website. [[Become a sponsor](https://opencollective.com/ponyc#sponsor)]

<a href="https://opencollective.com/ponyc/sponsor/0/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/0/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/1/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/1/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/2/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/2/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/3/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/3/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/4/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/4/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/5/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/5/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/6/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/6/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/7/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/7/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/8/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/8/avatar.svg"></a>
<a href="https://opencollective.com/ponyc/sponsor/9/website" target="_blank"><img src="https://opencollective.com/ponyc/sponsor/9/avatar.svg"></a>

# Getting help

Need help? Not to worry, we have you covered.

We have a couple resources designed to help you learn, we suggest starting with the tutorial and from there, moving on to the Pony Patterns book. Additionally, standard library documentation is available online.

* [Tutorial](http://tutorial.ponylang.io).
* [Pony Patterns](http://patterns.ponylang.io) cookbook is in progress
* [Standard library docs](http://stdlib.ponylang.io/).
* [Build Problems, see FAQ Compiling](https://www.ponylang.io/faq/#compiling).

If you are looking for an answer "right now", we suggest you give our [Zulip](https://ponylang.zulipchat.com/#narrow/stream/189985-beginner-help) community a try. Whatever your question is, it isn't dumb, and we won't get annoyed.

Think you've found a bug? Check your understanding first by writing the mailing list. Once you know it's a bug, open an issue.
* [Open an issue](https://github.com/ponylang/ponyc/issues)

# Trying it online

If you want a quick way to test or run code, checkout the [Playground](https://playground.ponylang.io/).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/ponylang/VS-pony)
* Visual Studio Code: [vscode-pony](https://marketplace.visualstudio.com/items?itemName=npruehs.pony)
* Vim:
    - [vim-pony](https://github.com/jakwings/vim-pony)
    - [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
    - [currycomb: Syntastic support](https://github.com/killerswan/pony-currycomb.vim)
    - [SpaceVim](http://spacevim.org), available as layer for Vim and [Neovim](https://neovim.io). Just follow [installation instructions](https://github.com/SpaceVim/SpaceVim) then load `lang#pony` layer inside configuration file (*$HOME/.SpaceVim.d/init.toml*)
* Emacs:
    - [ponylang-mode](https://github.com/ponylang/ponylang-mode)
    - [flycheck-pony](https://github.com/ponylang/flycheck-pony)
    - [pony-snippets](https://github.com/ponylang/pony-snippets)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)
* Micro: [micro-pony-plugin](https://github.com/Theodus/micro-pony-plugin)
* Nano: [pony.nanorc file](https://github.com/serialhex/nano-highlight/blob/master/pony.nanorc)
* Kate: update syntax definition file: Settings -> Configure Kate -> Open/Save -> Modes & Filetypes -> Download Highlighting Files

# Installation

Pony supports LLVM 3.9 and on an experimental basis it supports LLVM 4.0 and 5.0.

Pony's prerequisites for CPU platforms are:

- Full support for 64-bit platforms
  - x86 and ARM CPUs only
  - See platforms listed in the Circle-CI build list at
    https://circleci.com/gh/ponylang/ponyc
- Partial support for 32-bit platforms
  - The `arm` and `armhf` architectures are tested via CI (Continuous
    Integration testing)
  - See platforms listed in the Circle-CI build list at
    https://circleci.com/gh/ponylang/ponyc
  - See also: GitHub issues
    [#2836](https://github.com/ponylang/ponyc/issues/2836)
    and
    [#1576](https://github.com/ponylang/ponyc/issues/1576)
    for more information.

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

By default, the Pony Docker image is compiled without support for [AVX CPU instructions](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions). For optimal performance on modern hardware, you should build your Pony installation from source.

## Linux using an AppImage package (via Bintray)

For most Linux distributions released after RHEL 7, the `release` builds are packaged and available on Bintray ([pony-language/ponyc-appimage/ponyc](https://bintray.com/pony-language/ponyc-appimage/ponyc)) as an AppImage.

The AppImage (www.appimage.org) format allow for an easy ability to use applications with minimal clutter added to your system. The applications are available in a single file and can be run after they're made executable. Additionally, AppImages allow for multiple versions of Pony to be used side by with no conflicts.

To install builds via AppImage, you need to go to Bintray and download the appropriate file for the version you want. After the file is downloaded, you need to make it executable using `chmod`.

### DEB and AVX2 Support

By default, the Pony AppImage package is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

## Linux using an RPM package (via COPR)

For Red Hat, CentOS, Oracle Linux, Fedora Linux, or OpenSuSE, the `release` builds are packaged and available on COPR ([ponylang/ponylang](https://copr.fedorainfracloud.org/coprs/ponylang/ponylang/)).

### Using `yum` for Red Hat, CentOS, Oracle Linux and other RHEL compatible systems:

```bash
yum copr enable ponylang/ponylang epel-7
yum install ponyc
```

See https://bugzilla.redhat.com/show_bug.cgi?id=1581675 for why `epel-7` is required on the command line.

### Using `DNF` for Fedora Linux:

```bash
dnf copr enable ponylang/ponylang
dnf install ponyc
```

### Using Zypper for OpenSuSE Leap 15:

```bash
zypper addrepo --refresh --repo https://copr.fedorainfracloud.org/coprs/ponylang/ponylang/repo/opensuse-leap-15.0/ponylang-ponylang-opensuse-leap-15.0.repo
wget https://copr-be.cloud.fedoraproject.org/results/ponylang/ponylang/pubkey.gpg
rpm --import pubkey.gpg
zypper install ponyc
```

### RPM and AVX2 Support

By default, the Pony RPM package is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

## Ubuntu and Debian Linux using a DEB package (via Bintray)

For Ubuntu and Debian Linux, the `release` builds are packaged and available on Bintray ([pony-language/ponylang-debian](https://bintray.com/pony-language/ponylang-debian)).

Install packages to allow `apt` to use a repository over HTTPS:

```bash
sudo apt-get install \
     apt-transport-https \
     ca-certificates \
     curl \
     gnupg2 \
     software-properties-common
```

Install builds via Apt (and install Ponylang's public key):

```bash
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys E04F0923 B3B48BDA
sudo add-apt-repository "deb https://dl.bintray.com/pony-language/ponylang-debian  $(lsb_release -cs) main"
sudo apt-get update
sudo apt-get -V install ponyc
```

### DEB and AVX2 Support

By default, the Pony DEB package is compiled without support for AVX CPU instructions. For optimal performance, you should build your Pony installation from source.

### Linux Mint

All steps to install Pony in Linux Mint are the same from Ubuntu, but you must use the Ubuntu package base (`xenial`, `bionic`) instead of the Linux Mint release.

Install pre-requisites and add the correct `apt` repository:

```bash
sudo apt-get install \
     apt-transport-https \
     ca-certificates \
     curl \
     gnupg2 \
     software-properties-common
```

```bash
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys E04F0923 B3B48BDA
source /etc/upstream-release/lsb-release
sudo add-apt-repository "deb https://dl.bintray.com/pony-language/ponylang-debian $DISTRIB_CODENAME main"
sudo apt-get update
sudo apt-get -V install ponyc
```

The same AVX2 support restrictions apply.

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

- Visual Studio 2019, 2017 or 2015 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2019, 2017 or 2015 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)), and
  - If using Visual Studio 2015, install the Windows 10 SDK (available [here](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)).
  - If using Visual Studio 2017 or 2019, install the "Desktop Development with C++" workload.
  - If using Visual C++ Build Tools 2017 or 2019, install the "Visual C++ build tools" workload, and the "Visual Studio C++ core features" individual component.
  - If using Visual Studio 2017 or 2019, or Visual C++ Build Tools 2017 or 2019, make sure the latest `Windows 10 SDK (10.x.x.x) for Desktop` will be installed.

Once you have installed the prerequisites, you can download the latest ponyc release from [bintray](https://dl.bintray.com/pony-language/ponyc-win/).

# Building ponyc from source

First of all, you need a compiler with decent C11 support. The following compilers are supported, though we recommend to use the most recent versions.

- GCC >= 4.7
- Clang >= 3.4
- MSVC >= 2015
- XCode Clang >= 6.0

When building ponyc from sources the LLVM installed on your system is used by default. Optionally, you may also build ponyc with LLVM from [sources](#building-ponyc-using-llvm-sources).

## Building ponyc using LLVM sources:

### Prerequisites:

- git >= 2.17, other versions may work but this is what has been tested.

### Instructions:

To compile Pony using LLVM sources on Linux add `-f Makefile-lib-llvm` to any of the examples below. For instance on Ubuntu the standard command line is simply `make`, to build ponyc using LLVM from sources the command line is `make -f Makefile-lib-llvm`. Alternatively you can create a symlink from Makefile to Makefile-lib-llvm, `ln -sf Makefile-lib-llvm Makefile`, and no changes would be needed to the commands. You can specify `llvm_target=llvm-6.0.0` on the command line and those sources will be used. For example `make -f Makefile-lib-llvm llvm_target=llvm-6.0.0`.

Typically you only need to build the LLVM sources once, as the `make clean` target does not cause the LLVM sources to be rebuilt. To rebuild everything use `make -f Makefile-lib-llvm clean-all && `make -f Makefile-lib-llvm`. There is also a distclean target, `make -f Makefle-lib-llvm distclean`, which will remove the llvm sources and they will be retrieved from the ponylang/llvm repo.

NOTE: If LLVM version < 5.0.0 is used, cpu feature `avx512f` is disabled automagically to avoid [LLVM bug 30542](https://bugs.llvm.org/show_bug.cgi?id=30542) otherwise the compiler crashes during the optimization phase.

#### Changing the commit associated with LLVM_CFG=llvm-default.cfg

When LLVM_CFG is not specified or it is llvm-default.cfg the commit associated with the `src` submodule is checked out as the llvm source to be built. To change to a different commit, for instance tag `llvmorg-8.0.0`, simply clone ponyc and change `lib/llvm/src` to the desired commit:
```
git clone --recurse-submodules  https://github.com/<you>/ponyc
cd ponyc
git checkout -b update-lib-llvm-src-to-llvmorg-8.0.0
cd lib/llvm/src
git checkout llvmorg-8.0.0
cd ../../../
```
If you already have ponyc checked out update/init `lib/llvm/src` submodule, if it hasn't been fetched, and then go into `lib/llvm/src` and checkout the desired commit:
```
git submodule update --init
cd lib/llvm/src
git checkout llvmorg-8.0.0
cd ../../../
```
#### Debug/test ....
Now build and test using `LLVM_CFG=llvm-default.cfg` and any other appropriate parameters:
```
make -j12 LLVM_CFG=llvm-default.cfg default_pic=true -f Makefile-lib-llvm
```
When satisfied create a commit pushing to your repo:
```
git add lib/llvm/src
git commit -m "Update submodule lib/llvm/src to llvmorg-8.0.0"
git push origin update-lib-llvm-src-to-llvmorg-8.0.0
```
See the [Submodule section of the git-scm book](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for more information.

## Building on Linux

Get the pony source from GitHub (For information on setting up Git, see https://help.github.com/articles/set-up-git/):
```bash
sudo apt install git
git clone git://github.com/ponylang/ponyc
```

[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

### Arch

Install pony dependencies:

```
pacman -S llvm make ncurses zlib
```

To build ponyc and compile and run helloworld:

```bash
cd ~/ponyc/
make default_pic=true
./build/release/ponyc examples/helloworld
./helloworld
```

### Debian Sid

Install pony dependencies:

```bash
sudo apt-get update
sudo apt-get install make gcc g++ git zlib1g-dev libncurses5-dev \
  llvm llvm-dev
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make default_pic=true
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
sudo apt-get install -y build-essential git zlib1g-dev libncurses5-dev llvm-3.9
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
### Ubuntu Bionic

```bash
sudo apt-get update
sudo apt-get install -y build-essential git zlib1g-dev libncurses5-dev llvm-3.9
```

Clone the ponyc repo:

```bash
cd ~/
git clone https://github.com/ponylang/ponyc.git
```

Build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make default_pic=true
./build/release/ponyc examples/helloworld
./helloworld
```

### Linux Mint

Instructions for Linux Mint are the same as the appropriate Ubuntu installation. However, an extra `llvm-3.9-dev` package is required for missing headers.

After installing the `llvm` package by following the appropriate steps for Ubuntu Trusty (Linux Mint 17), Xenial (Linux Mint 18), or Bionic (Linux Mint 19), install the extra headers:

```bash
sudo apt-get install -y llvm-3.9-dev
```

### Fedora (25)

```bash
dnf check-update
sudo dnf install git gcc-c++ make zlib-devel llvm-devel ncurses-devel
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Fedora (26, 27, 28, Rawhide)

```bash
dnf check-update
sudo dnf install git gcc-c++ make zlib-devel llvm3.9-devel ncurses-devel \
 libatomic
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### CentOS/RHEL (7)

#### Install dependencies:

```bash
sudo yum install git gcc-c++ make zlib-devel ncurses-devel libatomic
```

Using LLVM 3.9.1 from EPEL:

```bash
wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
sudo yum install ./epel-release-latest-7.noarch.rpm
sudo yum install llvm3.9-devel llvm3.9-static
```

Using LLVM 3.9.1 from copr:

```bash
sudo yum install yum-plugin-copr
sudo yum copr enable alonid/llvm-3.9.1
sudo yum install llvm-3.9.1 llvm-3.9.1-devel llvm-3.9.1-static
```

Using LLVM 5.0.1 from copr:

```bash
sudo yum install yum-plugin-copr
sudo yum copr enable alonid/llvm-5.0.1
sudo yum install llvm-5.0.1 llvm-5.0.1-devel llvm-5.0.1-static
```

Using LLVM 4.0.1 from llvm-toolset-7 from SCL:

CentOS:
```bash
# 1. Install a package with repository for your system:
# On CentOS, install package centos-release-scl available in CentOS repository:
sudo yum install centos-release-scl
```

RHEL:
```bash
# On RHEL, enable RHSCL repository for you system:
sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
```

```bash
# 2. Install the collection:
sudo yum install llvm-toolset-7 llvm-toolset-7-llvm-devel llvm-toolset-7-llvm-static
```

Enable the llvm collection before building:
```bash
scl enable llvm-toolset-7 bash
```

#### To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make use="llvm_link_static"
./build/release/ponyc examples/helloworld
./helloworld
```

### OpenSUSE (Leap 42.3)

```bash
sudo zypper addrepo http://download.opensuse.org/repositories/devel:tools:compiler/openSUSE_Leap_42.3/devel:tools:compiler.repo
sudo zypper refresh
sudo zypper update
sudo zypper install git gcc-c++ make zlib-devel \
  llvm3_9-devel binutils-gold
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### OpenSuSE (Leap 15, Tumbleweed)

NOTE: LLVM 3.9 doesn't seem to be available so these instructions install the default LLVM version available.

```bash
sudo zypper install git gcc-c++ make zlib-devel \
  llvm-devel binutils-gold
```

To build ponyc, compile and run helloworld:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

### Alpine (3.6, 3.7, Edge)

Install build tools/dependencies:

```bash
apk add --update alpine-sdk binutils-gold llvm3.9 llvm3.9-dev \
  libexecinfo-dev coreutils linux-headers zlib-dev ncurses-dev
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

* LLVM 3.9.1
* zlib
* ncurses

There is experimental support for LLVM 4.0.1 and 5.0.0, but this may
result in decreased performance or crashes in generated applications.

To build ponyc, compile and run the hello world app:

```bash
cd ~/ponyc/
make
./build/release/ponyc examples/helloworld
./helloworld
```

## Building on DragonFly

Pony previously worked on DragonFly, however, at this time, while it builds, it doesn't pass all tests. If you'd be interested in getting Pony working on DragonFly, have at it. You'll need the following dependencies:

First, install the required dependencies:

```bash
sudo pkg install gmake
sudo pkg install llvm60
```

## Building on FreeBSD

First, install the required dependencies:

* Pony is currently supported on FreeBSD12 for amd64 (64-bit) using LLVM 7
* The following packages, with their installation commands:

```bash
sudo pkg install gmake
sudo pkg install llvm70
sudo pkg install libunwind
```

This will build ponyc and compile helloworld:

```bash
gmake
./build/release/ponyc examples/helloworld
```

## Building on OpenBSD

OpenBSD has been tested on OpenBSD 6.5.

First, install the required dependencies:

```bash
doas pkg_add gmake libexecinfo llvm
```

This will build ponyc and compile helloworld:

```bash
gmake verbose=true bits=64
./build/release/ponyc examples/helloworld
```

If you are on a 32-bit platform (e.g., armv7), change `bits=64` to `bits=32`.

## Building on Mac OS X
[![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

You'll need llvm 3.9.1 to build Pony. You can use either homebrew or MacPorts to install your dependencies.

There is experimental support for LLVM 4.0.1 or 5.0.0, but this may result in
decreased performance or crashes in generated applications.

Installation via [homebrew](http://brew.sh):
```
brew update
brew install llvm@3.9
```

Installation via [MacPorts](https://www.macports.org):
```
sudo port install llvm-3.9
sudo port select --set llvm mp-llvm-3.9
```

Launch the build with `make` after installing the dependencies:
```
make
./build/release/ponyc examples/helloworld
```

## Building on Windows
[![Backers on Open Collective](https://opencollective.com/ponyc/backers/badge.svg)](#backers) [![Sponsors on Open Collective](https://opencollective.com/ponyc/sponsors/badge.svg)](#sponsors) [![Windows](https://ci.appveyor.com/api/projects/status/kckam0f1a1o0ly2j?svg=true)](https://ci.appveyor.com/project/sylvanc/ponyc)

_Note: it may also be possible (as tested on build 14372.0 of Windows 10) to build Pony using the [Ubuntu 14.04](#ubuntu-1404-1510-1604) instructions inside [Bash on Ubuntu on Windows](https://msdn.microsoft.com/en-us/commandline/wsl/about)._

Building on Windows requires the following:

- Visual Studio 2019, 2017 or 2015 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2019, 2017 or 2015 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)), and
  - If using Visual Studio 2015, install the Windows 10 SDK (available [here](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)).
  - If using Visual Studio 2017 or 2019, install the "Desktop Development with C++" workload.
  - If using Visual C++ Build Tools 2017 or 2019, install the "Visual C++ build tools" workload, and the "Visual Studio C++ core features" individual component.
  - If using Visual Studio 2017 or 2019, or Visual C++ Build Tools 2017 or 2019, make sure the latest `Windows 10 SDK (10.x.x.x) for Desktop` will be installed.
- [Python](https://www.python.org/downloads) (3.6 or 2.7) needs to be in your PATH.

In a command prompt in the `ponyc` source directory, run the following:

```
make.bat configure
```

(You only need to run `make.bat configure` the first time you build the project.)

```
make.bat build test
```

This will automatically perform the following steps:

- Download the pre-built LLVM library for building the Pony compiler.
  - [LLVM](http://llvm.org)
- Build the pony compiler in the `build/<config>-<llvm-version>` directory.
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

This functionality boils down to "super LTO" for the runtime. The Pony compiler will have full knowledge of the runtime and will perform advanced interprocedural optimisations between your Pony code and the runtime. If you're looking for maximum performance, you should consider this option. Note that this can result in very long optimisation times.

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

This will result in an error message plus a listing of all architecture types acceptable on your platform.
