# Building ponyc from source

First of all, you need a compiler with decent C11 support. We officially support Clang on Unix and MSVC on Windows; the following are known to work:

- Clang >= 3.4
- XCode Clang >= 6.0
- MSVC >= 2017
- GCC >= 4.7

You also need [CMake](https://cmake.org/download/) version 3.21 or higher. You also need a version of [Python 3](https://www.python.org/downloads/) installed; it's required in order to build LLVM.

## Clone this repository

The Pony build process uses git submodules so you need to build from a checked out clone of this repository.

## Build Steps

The build system uses CMake, and includes a helper wrapper that will automatically set up your out-of-source build directories and libraries for you.

The build system is divided into several stages:

- Build the vendored LLVM libraries that are included in the `lib/llvm/src` Git submodule by running `make libs` (`.\make.ps1 libs` on Windows).  This stage only needs to be run once the first time you build (or if the vendored LLVM submodule changes, or if you run `make distclean`).
  - This can take a while. To ensure it's using all cores, try `make libs build_flags="-j6"`, replacing `6` with the number of CPU cores available.

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

Note that you only need to run `gmake libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## DragonFly

```bash
pkg install -y cxx_atomics
```

Then continue with the same instructions as FreeBSD.

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
Alpine 3.17+ | binutils-gold, clang, clang-dev, cmake, make
CentOS 8 | clang, cmake, diffutils, libatomic, libstdc++-static, make, zlib-devel
Fedora | clang, cmake, libatomic, libstdc++-static, make
Fedora 41 | binutils-gold, clang, cmake, libatomic, libstdc++-static, make
OpenSuse Leap | binutils-gold, cmake
Raspbian 32-bit | cmake
Raspbian 64-bit | cmake, clang
Rocky | clang, cmake, diffutils, libatomic, libstdc++-static, make, zlib-devel
Ubuntu | clang, cmake, make
Void | clang, cmake, make, libatomic libatomic-devel

Note that you only need to run `make libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### 32-bit Raspbian

Installing on a 32-bit Raspbian is slightly different from other Linux based
Operating Systems. There are two important things to note:

- at the moment, only `gcc` can be used to build Pony; `clang` currently doesn't work.
- you'll need to override the `tune` option to configure.

```bash
make libs
make configure tune=native
make build
sudo make install
```

### 64-bit Raspbian

Installing on a 64-bit Raspbian is slightly different from other Linux based
Operating Systems, you'll need to override the `arch` option to configure, but otherwise, everything is the same.

```bash
make libs pic_flag=-fPIC
make configure arch=armv8-a pic_flag=-fPIC
make build
sudo make install arch=armv8-a
```

### Asahi

Installing on Asahi is slightly different due to running on the M1 processor. You'll need to override the `arch` option to configure, but otherwise, everything is the same.

```bash
make libs
make configure arch=armv8
make build
sudo make install
```

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
- [Python 3](https://www.python.org/downloads/)
- Visual Studio 2019 or 2017 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2019 or 2017 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - If using Visual Studio, install the `Desktop Development with C++` workload.
  - If using Visual C++ Build Tools, install the `Visual C++ build tools` workload, and the `Visual Studio C++ core features` individual component.
  - Install the latest `Windows 10 SDK (10.x.x.x) for Desktop` component.

In a PowerShell prompt, run:

```powershell
.\make.ps1 libs
.\make.ps1 configure
.\make.ps1 build
```

Following building, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<ponyc repo>\build\release"`

Note that you only need to run `.\make.ps1 libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

---

## Additional Build Options on Unix

### arch

You can specify the CPU architecture to build Pony for via the `arch` make option:

```bash
make configure arch=arm7
make build
```

## dtrace

BSD and Linux based versions of Pony support using DTrace and SystemTap for collecting Pony runtime events.

DTrace support is enabled by setting `use=dtrace` in the build command line like:

```bash
make configure use=dtrace
make build
```

### lto

Link-time optimizations provide a performance improvement. You should strongly consider turning on LTO if you build ponyc from source. It's off by default as it comes with some caveats:

- If you aren't using clang as your linker, we've seen LTO generate incorrect binaries. It's rare but it can happen. Before turning on LTO you need to be aware that it's possible.

- If you are on MacOS, turning on LTO means that if you upgrade your version of XCode, you will have to rebuild your Pony compiler. You won't be able to link Pony programs if there is a mismatch between the version of XCode used to build the Pony runtime and the version of XCode you currently have installed.

LTO is enabled by setting `lto` to `yes` in the build command line like:

```bash
make configure lto=yes
make build
```

### runtime-bitcode

If you're compiling with Clang, you can build the Pony runtime as an LLVM bitcode file by setting `runtime-bitcode` to `yes` in the build command line:

```bash
make configure runtime-bitcode=yes
make build
```

Then, you can pass the `--runtimebc` option to ponyc in order to use the bitcode file instead of the static library to link in the runtime:

```bash
ponyc --runtimebc
```

This functionality boils down to "super LTO" for the runtime. The Pony compiler will have full knowledge of the runtime and will perform advanced interprocedural optimisations between your Pony code and the runtime. If you're looking for maximum performance, you should consider this option. Note that this can result in very long optimisation times.

### systematic testing

Systematic testing allows for running of Pony programs in a deterministic manner. It accomplishes this by coordinating the interleaving of the multiple runtime scheduler threads in a deterministic and reproducible manner instead of allowing them all to run in parallel like happens normally. This ability to reproduce a particular runtime behavior is invaluable for debugging runtime issues.

Systematic testing is enabled by setting `use=scheduler_scaling_pthreads,systematic_testing` in the build command line like:

```bash
make configure use=scheduler_scaling_pthreads,systematic_testing
make build
```

More information about systematic testing can be found in [SYSTEMATIC_TESTING.md](SYSTEMATIC_TESTING.md).

## Compiler Development

To ease development and support LSP tools like [clangd](https://clangd.llvm.org), create a `compile_commands.json` file with the following steps:

1. Run the `make configure` step for building ponyc with the following variable defined: `CMAKE_FLAGS='-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'`
2. symlink the generated `compile_commands.json` files into the project root directory:

  ```bash
  ln -sf build/build_debug/compile_commands.json compile_commands.json
  ```

  Replace `build_debug` with `build_release` is you are using a release configuration for compilation.

Now [clangd](https://clangd.llvm.org) will pick up the generated file and will be able to respond much quicker than without `compile_commands.json` file.
