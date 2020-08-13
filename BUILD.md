# Building ponyc from source

First of all, you need a compiler with decent C11 support. We officially support Clang on Unix and MSVC on Windows; the following are known to work:

- Clang >= 3.4
- XCode Clang >= 6.0
- MSVC >= 2017
- GCC >= 4.7

You also need [CMake](https://cmake.org/download/) version 3.15 or higher.

## Build Steps

The build system uses CMake, and includes a helper wrapper that will automatically set up your out-of-source build directories and libraries for you.

The build system is divided into several stages:

- Build the vendored LLVM libraries that are included in the `lib/llvm/src` Git submodule by running `make libs` (`.\make.ps1 libs` on Windows).  This stage only needs to be run once the first time you build (or if the vendored LLVM submodule changes, or if you run `make distclean`).

- `make configure` to configure the CMake build directory.  Use `make configure config=debug` (`.\make.ps1 configure -Config Debug`) for a debug build.
- `make build` will build ponyc and put it in `build/release`.  Use `make build config=debug` (`.\make.ps1 build -Config Debug` on Windows) for a debug build that goes in `build/debug`.
- `make test` will run the test suite.
- `make install` will install ponyc to `/usr/local` by default (`make install prefix=/foo` to install elsewhere; `make install -Prefix foo` on Windows).
- `make clean` will clean your ponyc build, but not the libraries.
- `make distclean` will delete the entire `build` directory, including the libraries.

The build system defaults to using Clang on Unix.  In order to use GCC, you must explicitly set it in the `configure` step: `make configure CC=gcc CXX=g++`.

## FreeBSD

```bash
pkg install -y cmake gmake libunwind git
gmake libs
gmake configure
gmake build
sudo gmake install
```

Note that you only need to run `make libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## Linux

```bash
make libs
make configure
make build
sudo make install
```

Additional Requirements:

Distribution | Requires
--- | ---
alpine | cmake, g++, make, libexecinfo
ubuntu | cmake, g++, make
fedora | cmake, gcc-c++, make, libatomic, libstdc++-static

Note that you only need to run `make libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## macOS

```bash
make libs
make configure
make build
sudo make install
```

Note that you only need to run `make libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## Windows

Building on Windows requires the following:

- [CMake](https://cmake.org/download/) version 3.15.0 or higher needs to be in your PATH.
- Visual Studio 2019 or 2017 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2019 or 2017 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - If using Visual Studio, install the `Desktop Development with C++` workload.
  - If using Visual C++ Build Tools, install the `Visual C++ build tools` workload, and the `Visual Studio C++ core features` individual component.
  - Install the latest `Windows 10 SDK (10.x.x.x) for Desktop` component.

In a PowerShell prompt, run:

```
.\make.ps1 libs
.\make.ps1 configure
.\make.ps1 build
```

Following building, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<ponyc repo>\build\release"`

Note that you only need to run `.\make.ps1 libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).


----

# Additional Build Options on Unix

## arch

You can specify the CPU architecture to build Pony for via the `arch` make option:

```bash
make arch=arm7
```

## lto

Link-time optimizations provide a performance improvement. You should strongly consider turning on LTO if you build ponyc from source. It's off by default as it comes with some caveats:

- If you aren't using clang as your linker, we've seen LTO generate incorrect binaries. It's rare but it can happen. Before turning on LTO you need to be aware that it's possible.

- If you are on MacOS, turning on LTO means that if you upgrade your version of XCode, you will have to rebuild your Pony compiler. You won't be able to link Pony programs if there is a mismatch between the version of XCode used to build the Pony runtime and the version of XCode you currently have installed.

LTO is enabled by setting `lto` to `yes` in the build command line like:

```bash
make configure lto=yes
make build
```

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

# Compiler Development

To ease development and support LSP tools like [clangd](https://clangd.llvm.org), create a `compile_commands.json` file with the following steps:

1. Run the `make configure` step for building ponyc with the following variable defined: `CMAKE_FLAGS='-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'`
2. symlink the generated `compile_commands.json` files into the project root directory:

  ```
  ln -sf build/build_libs/compile_commands.json compile_commands.json
  ```

  Replace `build_debug` with `build_release` is you are using a release configuration for compilation.

Now [clangd](https://clangd.llvm.org) will pick up the generated file and will be able to respond much quicker than without `compile_commands.json` file.
