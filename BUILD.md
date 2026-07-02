# Building ponyc from source

First of all, you need a compiler with decent C11 support. We officially support Clang on Unix and MSVC on Windows; the following are known to work:

- Clang >= 3.4
- XCode Clang >= 6.0
- MSVC >= 2017
- GCC >= 4.7

You also need [CMake](https://cmake.org/download/) version 3.21 or higher. You also need a version of [Python 3](https://www.python.org/downloads/) installed; it's required in order to build LLVM. On Unix systems, you need the zlib development headers and library installed (e.g. `zlib-dev`, `zlib1g-dev`, or `zlib-devel` depending on your distribution).

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
pkg install -y cmake gmake libunwind git python3
gmake libs
gmake configure
gmake build
sudo gmake install
```

Note that you only need to run `gmake libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## OpenBSD

```bash
pkg_add cmake gmake git python%3
gmake libs
gmake configure
gmake build
doas gmake install
```

Note that you only need to run `gmake libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### Unsupported OpenBSD build options

Several `use=` build options aren't supported on OpenBSD, because OpenBSD doesn't ship the runtime, headers, or tooling they depend on:

- `use=address_sanitizer` and `use=thread_sanitizer` — OpenBSD's base clang rejects `-fsanitize=address`/`-fsanitize=thread`; there is no AddressSanitizer or ThreadSanitizer runtime in base.
- `use=undefined_behavior_sanitizer` — OpenBSD ships only the minimal UndefinedBehaviorSanitizer runtime (`libclang_rt.ubsan_minimal.a`), not the standalone runtime ponyc links against, so the link fails.
- `use=coverage` — OpenBSD's base profiling runtime (`libclang_rt.profile.a`) is incomplete, so coverage-instrumented builds fail to link.
- `use=valgrind` — Valgrind has no OpenBSD port, so its development headers aren't available to build against.
- `use=dtrace` — not supported on OpenBSD.

`gmake configure` rejects these uses on OpenBSD with an error rather than letting the build fail partway through with a confusing compiler, linker, or missing-tool message.

## DragonFly

DragonFly BSD's base compiler (GCC 8.3) cannot build the vendored LLVM. Install GCC 13 and the required atomics package, then build with the packaged compiler:

```bash
pkg install -y cmake gmake git python3 cxx_atomics gcc13
gmake libs CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13
gmake configure CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13
gmake build CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13
sudo gmake install
```

Note that you only need to run `gmake libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### Unsupported DragonFly BSD build options

The sanitizer `use=` build options aren't supported on DragonFly, because the `gcc13` toolchain ponyc builds with doesn't ship their runtimes (GCC's libsanitizer has no DragonFly target, so no `libasan`/`libtsan`/`libubsan` is built):

- `use=address_sanitizer` — no AddressSanitizer runtime, so the link fails (`cannot find -lasan`).
- `use=thread_sanitizer` — no ThreadSanitizer runtime, so the link fails (`cannot find -ltsan`).
- `use=undefined_behavior_sanitizer` — no UndefinedBehaviorSanitizer runtime, so the link fails (`cannot find -lubsan`).

`gmake configure` rejects these uses on DragonFly with an error rather than letting the build fail partway through with a confusing linker message.

`use=dtrace` isn't supported on DragonFly either, and `gmake configure` rejects it.

`use=coverage` works on DragonFly: ponyc splices the gcc coverage runtime (`libgcov`) into the Pony programs it links, so coverage-instrumented programs build and run.

`use=valgrind` isn't rejected but doesn't work on DragonFly: it builds, and the Pony programs it compiles link, but DragonFly ships Valgrind 3.15, which is too old to run a Pony program ([#5435](https://github.com/ponylang/ponyc/issues/5435)).

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
Alpine 3.17+ | clang, clang-dev, cmake, make, zlib-dev
CentOS 8 | clang, cmake, diffutils, libatomic, libstdc++-static, make, zlib-devel
Fedora | clang, cmake, libatomic, libstdc++-static, make, zlib-devel
Fedora 41 | clang, cmake, libatomic, libstdc++-static, make, zlib-devel
OpenSuse Leap | cmake, zlib-devel
Raspbian 32-bit | cmake, zlib1g-dev
Raspbian 64-bit | cmake, clang, zlib1g-dev
Rocky | clang, cmake, diffutils, libatomic, libstdc++-static, make, zlib-devel
Ubuntu | clang, cmake, make, zlib1g-dev
Void | clang, cmake, make, libatomic, libatomic-devel, zlib-devel

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

## Haiku

You'll need additional, non-default software packages on Haiku:

- cmake
- python3.14
- libexecinfo_devel

A quick way to install those is to run following command in the Terminal:

```bash
pkgman install cmake python3.14 libexecinfo_devel
```

Once that's done, rest of the steps are similar to other operating systems:

```bash
make libs
make configure CC=gcc CXX=g++
make build
make install
```

Note that you only need to run `make libs` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## Windows

Building on Windows requires the following:

- [CMake](https://cmake.org/download/) version 3.15.0 or higher needs to be in your PATH.
- [Python 3](https://www.python.org/downloads/)
- Visual Studio 2022 or 2019 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2022 or 2019 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - If using Visual Studio, install the `Desktop Development with C++` workload.
  - If using Visual C++ Build Tools, install the `Visual C++ build tools` workload, and the `Visual Studio C++ core features` individual component.
  - Install the latest `Windows 11 SDK (11.x.x.x) for Desktop` component.

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

### llvm_tools

`make libs` builds the LLVM command-line tools (`llc`, `opt`, `llvm-link`, the various `llvm-*` utilities, and the standalone `lld`/`clang` driver binaries) by default. ponyc links LLVM and LLD as static libraries and never runs these binaries, so for a normal build they are dead weight — roughly 1.6 GB in `build/libs/bin` and a significant share of the libs build time. If you don't need them for local LLVM debugging, omit them by setting `llvm_tools` to `false`:

```bash
make libs llvm_tools=false
```

The default is `true`. The value only takes effect on a fresh libs build, so run `make cleanlibs` first if you have already built the libraries. Building the runtime as bitcode (see [runtime-bitcode](#runtime-bitcode) below) needs `llvm-link`, which is one of the omitted tools, so build the libs with `llvm_tools=true` if you intend to use it.

### arch

You can specify the CPU architecture to build Pony for via the `arch` make option:

```bash
make configure arch=arm7
make build
```

### sanitizers

ponyc can be built with the Clang/LLVM sanitizers to catch bugs in the compiler and the Pony runtime at runtime. Three `use=` options are available: `address_sanitizer` (AddressSanitizer — buffer overflows, use-after-free, double-free), `thread_sanitizer` (ThreadSanitizer — data races), and `undefined_behavior_sanitizer` (UndefinedBehaviorSanitizer — signed integer overflow, misaligned access, and other undefined behavior). AddressSanitizer and ThreadSanitizer can't be combined; either can be combined with UndefinedBehaviorSanitizer.

Pair `address_sanitizer` with `pool_memalign` so AddressSanitizer can track the Pony runtime's own allocations: `pool_memalign` routes every runtime allocation through `posix_memalign`/`free`, which AddressSanitizer intercepts and surrounds with redzones. The combination CI builds and tests is:

```bash
make configure config=debug use=pool_memalign,address_sanitizer,undefined_behavior_sanitizer
make build config=debug
```

The build lands in a suffixed directory — `build/debug-address_sanitizer-undefined_behavior_sanitizer-pool_memalign` for the configuration above. The suffix lists the enabled options in a fixed order set by CMake, independent of the `use=` order. These commands use `config=debug`; the sanitizers build under `config=release` too, and CI tests both.

The instrumented `ponyc` runs both during its own build (it compiles the self-hosted tools) and every time you use it to compile a program, so AddressSanitizer's runtime options must be set in the environment for `make build` and for any later `ponyc` invocation:

- ponyc isn't LeakSanitizer-clean, so `detect_leaks=0` suppresses the compiler's own leak reports at exit.
- On platforms whose base system C++ runtime is libc++ (FreeBSD, macOS), the instrumented compiler shares `std::vector`/`std::string` with the non-instrumented vendored LLVM, and libc++'s container annotations then abort on false-positive container overflows; `detect_container_overflow=0` suppresses them. Linux uses libstdc++ and doesn't need it.

Platform | `ASAN_OPTIONS`
--- | ---
Linux | `detect_leaks=0`
FreeBSD, macOS | `detect_leaks=0:detect_container_overflow=0`

Both options concern `ponyc` itself, not the programs it compiles. A correct Pony program is LeakSanitizer-clean at exit, so run a program you compiled with LeakSanitizer left on — it will report genuine leaks in your own code, such as memory you allocate through FFI and never free. The container-overflow false positive is likewise specific to the compiler's libc++/LLVM sharing; a compiled program links the C runtime, not libc++ or LLVM, so it can't occur there.

For example, on Linux:

```bash
ASAN_OPTIONS=detect_leaks=0 make build config=debug
ASAN_OPTIONS=detect_leaks=0 ./build/debug-address_sanitizer-undefined_behavior_sanitizer-pool_memalign/ponyc path/to/your/program
```

The sanitizers can't be built on OpenBSD or DragonFly BSD; see [Unsupported OpenBSD build options](#unsupported-openbsd-build-options) and [Unsupported DragonFly BSD build options](#unsupported-dragonfly-bsd-build-options).

### dtrace

Linux, FreeBSD, and macOS support collecting Pony runtime events, through SystemTap on Linux and DTrace on FreeBSD and macOS. DTrace isn't supported on DragonFly BSD or OpenBSD.

On macOS, actually tracing a running program with `dtrace` requires System Integrity Protection (SIP) to permit DTrace. See the [examples/dtrace README](examples/dtrace/README.md) for details.

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

This requires `llvm-link`, which is one of the LLVM tools `make libs` builds by default. If you built the libraries with [`llvm_tools=false`](#llvm_tools), rebuild them with the tools (`make cleanlibs && make libs`) before configuring with `runtime-bitcode=yes`.

This functionality boils down to "super LTO" for the runtime. The Pony compiler will have full knowledge of the runtime and will perform advanced interprocedural optimisations between your Pony code and the runtime. If you're looking for maximum performance, you should consider this option. Note that this can result in very long optimisation times.

`--runtimebc` cannot be combined with a compiler built using `use=dtrace`. The bitcode runtime has no DTrace/SystemTap probes (probe generation works on native object files, not bitcode), so ponyc rejects the combination with an error.

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
