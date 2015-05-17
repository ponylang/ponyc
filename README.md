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
* Emacs: [ponylang-mode](https://github.com/abingham/ponylang-mode)
* BBEdit: [bbedit-pony](https://github.com/TheMue/bbedit-pony)

# Installation
## Mac OSX using [Homebrew](http://brew.sh)

```bash 
$ brew install ponyc
```

A pull request for the ponyc formula to be part of homebrew-core is [pending](https://github.com/Homebrew/homebrew/pull/39192).

## Linux

* ```ponyc```: Recommended. Should work on most modern ```x86_64``` platforms.
* ```ponyc-avx2```: For platforms with AVX2 support. 
* ```ponyc-numa```: A numa-aware version of ```ponyc-avx2```.

### Apt-get and Aptitude

First, import the public key of ponylang.org:

```bash
$ wget -O - http://www.ponylang.org/releases/buildbot@lists.ponylang.org.gpg.key | sudo apt-key add -
```

Add the ponylang.org repository to apt-get:

```bash
sudo add-apt-repository "deb http://ponylang.org/releases/apt ponyc main"
sudo add-apt-repository "deb http://ponylang.org/releases/apt ponyc-avx2 main"
sudo add-apt-repository "deb http://ponylang.org/releases/apt ponyc-numa main"
```

Note that ```add-apt-repository``` may require to install ```python-software-properties``` or ```software-properties-common```.

Then, update your repository cache:

```bash
$ sudo apt-get update
```

Install ```ponyc```, ```ponyc-avx2``` or ```ponyc-numa```:

```bash
$ sudo apt-get install <package name>
```

### Zypper

First, add the ponylang.org repository:

```bash
$ sudo zypper ar -f http://www.ponylang.org/releases/yum/ponyc.repo
```

Install ```ponyc```, ```ponyc-avx2``` or ```ponyc-numa```:

```bash
$ sudo zypper install <package-name>
```

### YUM

First, add the ponylang.org repository:

```bash
$ sudo yum-config-manager --add-repo=http://www.ponylang.org/releases/yum/ponyc.repo
```

Install ```ponyc```, ```ponyc-avx2``` or ```ponyc-numa```:

```bash
$ sudo yum install <package-name>
```

## Windows

64-Bit installers for Windows 7, 8, 8.1 and 10 will be available soon.

## Download installers

All installers can also be downloaded from ponylang.org's servers:

* [Ubuntu/Debian](http://ponylang.org/releases/debian)
* [RPM](http://ponylang.org/releases/yum)
* [Windows](http://ponylang.org/releases/windows)

# Building on Linux [![build status](http://ponylang.org:50000/buildStatus/icon?job=ponyc)](http://ci.ponylang.org/job/ponyc/)

First, install LLVM 3.6 using your package manager. You may need to install zlib and ncurses as well.

This will build ponyc and compile helloworld:

```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

To build a NUMA-aware runtime, install libnuma-dev using your package manager and build as follows:

```
$ make use=numa config=release
```

# Building on Mac OSX

First, install [homebrew](http://brew.sh) if you haven't already. Then, brew llvm36, like this:

```
$ brew tap homebrew/versions
$ brew install llvm36
```

This will build ponyc and compile helloworld:

```
$ make config=release
$ ./build/release/ponyc examples/helloworld
```

# Building on Windows

The LLVM 3.7 (not 3.6!) prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.7 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

You will also need to build and install premake5 (not premake4) from source. We need premake5 in order to support current versions of Visual Studio.

You may also need to install zlib and ncurses.

```
$ premake5 vs2013
$ Release build with Visual Studio (ponyc.sln)
$ ./build/release/ponyc examples/helloworld
```

