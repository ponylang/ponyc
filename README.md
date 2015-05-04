# Getting help 

* [Open an issue!](https://github.com/CausalityLtd/ponyc/issues)

* Use the [mailing list](mailto:ponydev@lists.ponylang.org).

* A tutorial is available [here](http://tutorial.ponylang.org).

# Editor support

* Sublime Text: [Pony Language](https://packagecontrol.io/packages/Pony%20Language)

* Atom: [language-pony](https://atom.io/packages/language-pony)

* Visual Studio: [VS-pony](https://github.com/CausalityLtd/VS-pony).

* Vim: pending.

# Installation
## Mac OSX using [Homebrew](http://brew.sh)

```bash
$ brew install http://www.ponylang.org/releases/ponyc.rb
$ ponyc --version
0.1.2
```

A pull request for the ponyc formula to be part of homebrew-core is [pending](https://github.com/Homebrew/homebrew/pull/39192).

## Linux

* ```ponyc```: This package is the recommended one and should work on most modern ```x86_64``` platforms.
* ```ponyc-noavx```: Ponyc for platforms without AVX support (for example certain virtual machines) 
* ```ponyc-numa```: A numa-aware version of ```ponyc```.

### Apt-get and Aptitude

First, you can trust the public key of ponylang.org:

```bash
$ wget -O - http://www.ponylang.org/releases/buildbot@lists.ponylang.org.gpg.key | sudo apt-key add -
```

Add the ponylang.org repository to apt-get:

```bash
deb http://ponylang.org/releases/apt ponyc main
deb http://ponylang.org/releases/apt ponyc-numa main
deb http://ponylang.org/releases/apt ponyc-noavx main
```

Then, update your repository cache:

```bash
$ sudo apt-get update
```

Install ```ponyc```, ```ponyc-noavx``` or ```ponyc-numa```:

```bash
$ sudo apt-get install <package name>
$ ponyc --version
0.1.2
```

### Zypper

First, add the ponylang.org repository:

```bash
$ sudo zypper ar -f http://www.ponylang.org/releases/yum ponyc
```

Install ```ponyc```, ```ponyc-noavx``` or ```ponyc-numa```:

```bash
$ sudo zypper install <package-name>
$ ponyc --version
0.1.2
```

### YUM

First, add the ponylang.org repository:

```bash
$ sudo yum-config-manager --add-repo=http://www.ponylang.org/releases/yum/ponyc.repo
```

Install ```ponyc```, ```ponyc-noavx``` or ```ponyc-numa```:

```bash
$ sudo yum install <package-name>
$ ponyc --version
0.1.2
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

The LLVM 3.6 prebuilt binaries for Windows do NOT include the LLVM development tools and libraries. Instead, you will have to build and install LLVM 3.6 from source. You will need to make sure that the path to LLVM/bin (location of llvm-config) is in your PATH variable.

You will also need to build and install premake5 (not premake4) from source. We need premake5 in order to support current versions of Visual Studio.

You may also need to install zlib and ncurses.

```
$ premake5 vs2013
$ Release build with Visual Studio (ponyc.sln)
$ ./build/release/ponyc examples/helloworld
```

