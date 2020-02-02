# Building ponyc from source

First of all, you need a compiler with decent C11 support. The following compilers are supported, though we recommend to use the most recent versions.

- GCC >= 4.7
- Clang >= 3.4
- MSVC >= 2015
- XCode Clang >= 6.0

## FreeBSD

```bash
pkg install -y cmake gmake libunwind git
make -f Makefile-lib-llvm
sudo make -f Makefile-lib-llvm install
```

## Linux

```bash
make -f Makefile-lib-llvm
sudo make -f Makefile-lib-llvm install
```

Additional Requirements:

Distribution | Requires
--- | ---
alpine | cmake, g++, make, libexecinfo
ubuntu | cmake, g++, make

## macOS

```bash
make -f Makefile-lib-llvm
sudo make -f Makefile-lib-llvm install
```

## Windows

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

Following building, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<ponyc repo>\build\release-llvm-7.0.1"`

----

# Additional Build options

## arch

You can specify the CPU architecture to build Pony for via the `arch` make option:

```bash
make arch=arm7
```

## lto

Link-time optimizations provide a performance improvement. You should strongly consider turning on LTO if you build ponyc from source. It's off by default as it comes with some caveats:

- If you aren't using clang as your linker, we've seen LTO generate incorrect binaries. It's rare but it can happen. Before turning on LTO you need to be aware that it's possible.

- If you are on MacOS, turning on LTO means that if you upgrade your version of XCode, you will have to rebuild your Pony compiler. You won't be able to link Pony programs if there is a mismatch between the version of XCode used to build the Pony runtime and the version of XCode you currently have installed.

You can enable LTO when building the compiler in release mode. There are slight differences between platforms so you'll need to do a manual setup. LTO is enabled by setting `lto` to `yes` in the build command line like:

```bash
make lto=yes
```

If the build fails, you have to specify the LTO plugin for your compiler in the `LTO_PLUGIN` variable. For example:

```bash
make LTO_PLUGIN=/usr/lib/LLVMgold.so
```

Refer to your compiler documentation for the plugin to use in your case.

## runtime-bitcode

If you're compiling with Clang, you can build the Pony runtime as an LLVM bitcode file by setting `runtime-bitcode` to `yes` in the build command line:

```bash
make runtime-bitcode=yes
```

Then, you can pass the `--runtimebc` option to ponyc in order to use the bitcode file instead of the static library to link in the runtime:

```bash
ponyc --runtimebc
```

This functionality boils down to "super LTO" for the runtime. The Pony compiler will have full knowledge of the runtime and will perform advanced interprocedural optimisations between your Pony code and the runtime. If you're looking for maximum performance, you should consider this option. Note that this can result in very long optimisation times.
