# Getting help 

* [Open an issue!](https://github.com/ponylang/ponyc/issues)
* Use the [mailing list](mailto:pony+user@groups.io).
* Join ```#ponylang``` on [freenode](http://freenode.net/irc_servers.shtml).
* A tutorial is available [here](http://tutorial.ponylang.org).
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

A live ebuild is also available in the [overlay](https://github.com/stefantalpalaru/gentoo-overlay) (dev-lang/pony-9999) and for Vim users there's app-vim/pony-syntax.

### Other distributions

We're transitioning to bintray. For now, please build from source.

## Windows

You will need to build from source.

# Building ponyc from source

## Building on Linux [![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

First, install LLVM 3.7.1 using your package manager. You may need to install
zlib, ncurses, pcre2, and ssl as well.

This will build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on FreeBSD

First, install the required dependencies:

```bash
sudo pkg install gmake
sudo pkg install llvm37
sudo pkg install pcre2
sudo pkg install libunwind
```

This will build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on Mac OS X [![Linux and OS X](https://travis-ci.org/ponylang/ponyc.svg?branch=master)](https://travis-ci.org/ponylang/ponyc)

You'll need llvm 3.7.1 and the pcre2 library to build Pony.

Either install them via [homebrew](http://brew.sh):
```
$ brew update
$ brew install homebrew/versions/llvm37 pcre2 libressl
```

Or install them via macport:
```
$ sudo port install llvm-3.7 pcre2 libressl
$ sudo port select --set llvm mp-llvm-3.7
```

Then launch the build with Make:
```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on Windows [![Windows](https://ci.appveyor.com/api/projects/status/kckam0f1a1o0ly2j?svg=true)](https://ci.appveyor.com/project/sylvanc/ponyc)

The LLVM 3.7 prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.7 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

You will also need to build and install premake5 (not premake4) from source. We need premake5 in order to support current versions of Visual Studio.

You may also need to install zlib and ncurses.

```
$ premake5 vs2013
$ Release build with Visual Studio (ponyc.sln)
$ ./build/release/ponyc examples/helloworld
```
