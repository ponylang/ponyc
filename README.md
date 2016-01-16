# Getting help 

* [Open an issue!](https://github.com/CausalityLtd/ponyc/issues)
* Use the [mailing list](mailto:ponydev@lists.ponylang.org).
* Join ```#ponylang``` on [freenode](http://freenode.net/irc_servers.shtml).
* A tutorial is available [here](http://tutorial.ponylang.org).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)
* Atom: [language-pony](https://atom.io/packages/language-pony)
* Visual Studio: [VS-pony](https://github.com/CausalityLtd/VS-pony)
* Vim: [pony.vim](https://github.com/dleonard0/pony-vim-syntax)
* Emacs:
    - [ponylang-mode](https://github.com/abingham/ponylang-mode)
    - [flycheck-pony](https://github.com/rmloveland/flycheck-pony)
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

## Building on Linux [![Linux and OS X](https://travis-ci.org/CausalityLtd/ponyc.svg?branch=master)](https://travis-ci.org/CausalityLtd/ponyc)

First, install LLVM 3.6 using your package manager. You may need to install zlib, ncurses, pcre2, and ssl as well.

This will build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on FreeBSD

First, install the required dependencies:

```bash
sudo pkg install gmake
sudo pkg install llvm36
sudo pkg install pcre2
sudo pkg install libunwind
```

This will build ponyc and compile helloworld:

```bash
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on Mac OS X [![Linux and OS X](https://travis-ci.org/CausalityLtd/ponyc.svg?branch=master)](https://travis-ci.org/CausalityLtd/ponyc)

First, install [homebrew](http://brew.sh) if you haven't already. Then, brew the required dependencies:

```
$ brew update
$ brew install llvm
$ brew install pcre2
```

This will build ponyc and compile helloworld:

```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

## Building on Windows [![Windows](https://ci.appveyor.com/api/projects/status/8q026e7byvaflvei?svg=true)](https://ci.appveyor.com/project/pony-buildbot/ponyc)

The LLVM 3.7 (not 3.6!) prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.7 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

You will also need to build and install premake5 (not premake4) from source. We need premake5 in order to support current versions of Visual Studio.

You may also need to install zlib and ncurses.

```
$ premake5 vs2013
$ Release build with Visual Studio (ponyc.sln)
$ ./build/release/ponyc examples/helloworld
```
