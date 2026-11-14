# Building ponyc from source

First of all, you need a compiler with decent C11 support. We officially support Clang on Unix and clang-cl on Windows; the following are known to work:

- Clang >= 3.4
- XCode Clang >= 6.0
- clang-cl >= 12.0
- GCC >= 4.7

You also need [CMake](https://cmake.org/download/) version 3.25 or higher. You also need a version of [Python 3](https://www.python.org/downloads/) installed; it's required in order to build LLVM. On Unix systems, you need the zlib development headers and library installed (e.g. `zlib-dev`, `zlib1g-dev`, or `zlib-devel` depending on your distribution). You also need an SSL development library — either OpenSSL or LibreSSL — because the standard library's `net` package requires it. CMake detects the installed library and version automatically at configure time. If it can't find your SSL installation (for example, a keg-only Homebrew formula on macOS, or a non-standard install path), point CMake at it with `-DOPENSSL_ROOT_DIR=/path/to/ssl` at the configure step.

## Clone this repository

The direct CMake build uses git submodules, so build from a checked out clone of this repository. The [Nix package](#nix-and-nixos) can also be built from a source archive without Git metadata.

## Build Steps

The build system uses CMake. Configuration presets in `CMakePresets.json` set up your out-of-source build directories for you, and a small CMake runner (`lib/build-libs.cmake`) builds the vendored LLVM and support libraries.

The build is divided into several stages:

- Build the vendored LLVM libraries that are included in the `lib/llvm/src` Git submodule by running `cmake -P lib/build-libs.cmake`.  This stage only needs to be run once the first time you build (or if the vendored LLVM submodule changes, or if you delete the `build` directory).
  - This can take a while. To use more cores, pass `-DJOBS=6`, replacing `6` with the number of CPU cores available (LLVM is memory-hungry, so keep it modest).  Note, This argument will be ignored if it is not placed before the "Process" (-P) flag: `cmake -DJOBS=6 -P lib/build-libs.cmake`.

- `cmake --preset release` to configure the build directory.  Use `cmake --preset debug` for a debug build.
- `cmake --build --preset release` will build ponyc and put it in `build/release`.  Use `cmake --build --preset debug` for a debug build that goes in `build/debug`.
- `ctest --preset release -L ci-core` will run the compiler, runtime, and standard library tests.
- `cmake --install build/build_release` will install ponyc to `/usr/local` by default (`cmake --install build/build_release --prefix /foo` to install elsewhere).
- Removing a configuration's build directory (`rm -rf build/build_release build/release`) cleans your ponyc build without touching the libraries.
- `rm -rf build` will delete the entire `build` directory, including the libraries.

ponyc builds in two configurations, `release` and `debug`. The presets set this for you. If you configure without a preset, set it yourself: `cmake -B mybuild -S . -DCMAKE_BUILD_TYPE=release` (or `debug`). The name is lowercase because it is also the name of the output directory (`build/release`, `build/debug`); any other value, including CMake's usual `Release`, `RelWithDebInfo`, and `MinSizeRel`, is rejected at configure time.

The presets default to using Clang on Unix. To build ponyc with GCC, override the compiler at the configure step:

```bash
cmake --preset release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
```

The libs build (`cmake -P lib/build-libs.cmake`) takes the same `-DCMAKE_C_COMPILER`/`-DCMAKE_CXX_COMPILER` flags — one spelling overrides the compiler for either step. You don't need to rebuild the libraries to switch the ponyc compiler; the ones built by the default (Clang) libs build work with a GCC ponyc build. Pass the override to the libs build on platforms that need a non-default compiler for the vendored LLVM: 32-bit Raspbian, DragonFly BSD, and the Nix development environment. See those sections for the commands.

## Nix and NixOS

The Nix package supports `x86_64-linux`. Enable `nix-command` and `flakes` in your Nix configuration:

```ini
experimental-features = nix-command flakes
```

You can also enable them for a single command by placing `--extra-experimental-features 'nix-command flakes'` before the subcommand, for example:

```bash
nix --extra-experimental-features 'nix-command flakes' build .#ponyc --no-update-lock-file
```

### Install from a source checkout

From the root of a checkout containing `flake.nix`, build the package:

```bash
nix build .#ponyc --no-update-lock-file
./result/bin/ponyc --version
```

To install the package in your Nix profile, run:

```bash
nix profile install .#ponyc --no-update-lock-file
ponyc --version
```

The package includes `ponyc`, `pony-lsp`, `pony-lint`, `pony-doc`, `pony-dep`, and `pony-agent-server`. You can run these commands outside `nix develop`; no external `LD_LIBRARY_PATH` setting is required. The package build uses a locked LLVM archive, independently of the LLVM submodule in your checkout.

### Install on NixOS

Add Pony to your system flake's inputs. Replace `REVISION` with a revision of `ponylang/ponyc` that contains `flake.nix`:

```nix
inputs.ponyc.url = "github:ponylang/ponyc/REVISION";
```

Pass the flake inputs to your NixOS modules through `specialArgs`. For example, in an existing system flake:

```nix
outputs = inputs@{ nixpkgs, ... }: {
  nixosConfigurations.myhost = nixpkgs.lib.nixosSystem {
    specialArgs = { inherit inputs; };
    modules = [ ./configuration.nix ];
  };
};
```

Then add the package in `configuration.nix`:

```nix
{ inputs, pkgs, ... }:
{
  environment.systemPackages = [
    inputs.ponyc.packages.${pkgs.stdenv.hostPlatform.system}.ponyc
  ];
}
```

The package uses its own committed lock file. There is no separate Pony NixOS module.

### Compiler environment and generated programs

The installed commands use the package's pinned GCC and libc, with the required OpenSSL, zlib, and resource headers and libraries. Their wrappers set `NIX_CC` to the package toolchain and prepend the required entries to `NIX_CFLAGS_COMPILE` and `NIX_LDFLAGS`. Setting `NIX_CC` in your shell does not select another compiler for these commands.

You can add include and library directories through `NIX_CFLAGS_COMPILE` and `NIX_LDFLAGS`. The parser accepts `-isystem PATH`, `-LPATH`, and `-L PATH`. Paths containing spaces and quoted flag syntax are unsupported. Include directories and library directories each have a limit of 64 entries. The package uses six entries in each list, leaving 58 caller entries per list; duplicates count toward the limit. Required package entries come first. Extra entries beyond the limit produce a warning and are ignored.

Generated ELF programs refer to runtime paths in the Nix store. Keep the compiler's profile installation or `result` link to retain its runtime closure. Arbitrary output files are not Nix garbage collection roots: manage their runtime references when retaining or distributing programs independently. These programs are not portable standalone binaries.

These environment guarantees apply to the Nix package. A raw CMake install follows a separate workflow.

### Contributor builds

Enter the locked development environment from the repository root:

```bash
nix develop --no-update-lock-file
```

This provides build dependencies without building LLVM or ponyc. It does not run build hooks or update dependencies. Run the following commands inside this environment as an unprivileged user. The `pony-dep` permission tests require filesystem access to be denied when permissions are removed; root bypasses that denial.

Use GCC for both the vendored libraries and ponyc. The Unix CMake presets default to Clang, whose Nix wrapper cannot find the static GCC `libstdc++.a` needed by this build.

Initialize the LLVM submodule and build the vendored libraries:

```bash
git submodule update --init lib/llvm/src
export CMAKE_BUILD_PARALLEL_LEVEL=2
cmake -DJOBS=2 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake
```

The first build compiles LLVM and requires substantial memory. Limit parallel builds to the memory available; the two-job limit above is an example, not a memory guarantee. Provide swap or more RAM for optimized stdlib compilation, which can exhaust an 8 GiB machine without swap even with one Pony compiler process.

Configure and build ponyc, then run the core tests with debug Pony code generation:

```bash
cmake --preset x86-64-debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset x86-64-debug
PONY_DEBUG=1 ctest --preset x86-64-debug -L ci-core --output-on-failure
```

Run the stdlib and full-program tests again with optimized Pony code generation:

```bash
env -u PONY_DEBUG ctest --preset x86-64-debug -R '^(stdlib|full-programs)$' --output-on-failure
```

Build the six tool test binaries explicitly, then run them:

```bash
cmake --build --preset x86-64-debug --target tool-tests
ctest --preset x86-64-debug -L tools --output-on-failure
```

For a release build:

```bash
cmake --preset x86-64-release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset x86-64-release
```

The CMake configuration directories are `build/build_x86-64-debug` and `build/build_x86-64-release`. The binaries are in `build/debug` and `build/release`.

These builds use the LLVM submodule in your checkout. The Nix package uses the separately pinned archive in `nix/sources.nix`; local submodule changes do not change the package build. Local Git flake builds include edits to tracked files. Stage new files with `git add` before building the package so Nix includes them.

To check the Nix package and its installed commands:

```bash
nix flake check --no-update-lock-file --print-build-logs
```

This builds the package and runs an independent check of the installed commands. The package checks include the C/C++ test suites and deterministic installed operations. They do not replace the full core and tool test commands above.

A raw CMake installation is also possible:

```bash
cmake --install build/build_x86-64-release --prefix /desired/prefix
```

This does not install the Nix package wrappers. Keep the development environment active when running these manually installed binaries.

### Updating the Nix dependencies

The committed `flake.lock` pins the development and package environment. Routine builds must preserve it. To intentionally update the flake inputs, run:

```bash
nix flake update
```

Inspect the lock diff. If the pinned LLVM source identity changes, regenerate the affected fixed-source hashes with Nix as well. Commit the lock file and affected hashes together.

After an update, enter the development environment again and repeat the contributor builds, core tests in both Pony code generation modes, tool tests, package build, and `nix flake check` above. Check that the installed commands and generated programs still work with their runtime closure, and verify cache restoration with the updated environment. Changes to the environment definitions, lock file, or bootstrap invalidate the Nix archive cache key; do not assume the previous archive will be reused. CI uses `--no-update-lock-file` and does not update the lock during builds.

## FreeBSD

```bash
pkg install -y cmake gmake libunwind git openssl python3
cmake -P lib/build-libs.cmake
cmake --preset release
cmake --build --preset release
sudo cmake --install build/build_release
```

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## OpenBSD

```bash
pkg_add cmake gmake git openssl python%3
cmake -P lib/build-libs.cmake
cmake --preset release
cmake --build --preset release
doas cmake --install build/build_release
```

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### Unsupported OpenBSD build options

Several `use=` build options aren't supported on OpenBSD, because OpenBSD doesn't ship the runtime, headers, or tooling they depend on:

- `use=address_sanitizer` and `use=thread_sanitizer` — OpenBSD's base clang rejects `-fsanitize=address`/`-fsanitize=thread`; there is no AddressSanitizer or ThreadSanitizer runtime in base.
- `use=undefined_behavior_sanitizer` — OpenBSD ships only the minimal UndefinedBehaviorSanitizer runtime (`libclang_rt.ubsan_minimal.a`), not the standalone runtime ponyc links against, so the link fails.
- `use=coverage` — OpenBSD's base profiling runtime (`libclang_rt.profile.a`) is incomplete, so coverage-instrumented builds fail to link.
- `use=valgrind` — Valgrind has no OpenBSD port, so its development headers aren't available to build against.

Configuring (`cmake --preset release`) rejects these uses on OpenBSD with an error rather than letting the build fail partway through with a confusing compiler, linker, or missing-tool message.

## DragonFly

DragonFly BSD's base compiler (GCC 8.3) cannot build the vendored LLVM. Install GCC 13 and the required atomics package, then build with the packaged compiler:

```bash
pkg install -y cmake gmake git openssl python3 cxx_atomics gcc13
cmake -DCMAKE_C_COMPILER=/usr/local/bin/gcc13 -DCMAKE_CXX_COMPILER=/usr/local/bin/g++13 -P lib/build-libs.cmake
cmake --preset release -DCMAKE_C_COMPILER=/usr/local/bin/gcc13 -DCMAKE_CXX_COMPILER=/usr/local/bin/g++13
cmake --build --preset release
sudo cmake --install build/build_release
```

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### Unsupported DragonFly BSD build options

The sanitizer `use=` build options aren't supported on DragonFly, because the `gcc13` toolchain ponyc builds with doesn't ship their runtimes (GCC's libsanitizer has no DragonFly target, so no `libasan`/`libtsan`/`libubsan` is built):

- `use=address_sanitizer` — no AddressSanitizer runtime, so the link fails (`cannot find -lasan`).
- `use=thread_sanitizer` — no ThreadSanitizer runtime, so the link fails (`cannot find -ltsan`).
- `use=undefined_behavior_sanitizer` — no UndefinedBehaviorSanitizer runtime, so the link fails (`cannot find -lubsan`).

Configuring rejects these uses on DragonFly with an error rather than letting the build fail partway through with a confusing linker message.

`use=coverage` works on DragonFly: ponyc splices the gcc coverage runtime (`libgcov`) into the Pony programs it links, so coverage-instrumented programs build and run.

`use=valgrind` isn't rejected but doesn't work on DragonFly: it builds, and the Pony programs it compiles link, but DragonFly ships Valgrind 3.15, which is too old to run a Pony program ([#5435](https://github.com/ponylang/ponyc/issues/5435)).

## Linux

```bash
cmake -P lib/build-libs.cmake
cmake --preset release
cmake --build --preset release
sudo cmake --install build/build_release
```

Additional Requirements:

Distribution | Requires
--- | ---
Alpine 3.17+ | clang, clang-dev, cmake, libressl-dev, make, zlib-dev
CentOS 8 | clang, cmake, diffutils, libatomic, libstdc++-static, make, openssl-devel, zlib-devel
Fedora | clang, cmake, libatomic, libstdc++-static, make, openssl-devel, zlib-devel
Fedora 41 | clang, cmake, libatomic, libstdc++-static, make, openssl-devel, zlib-devel
OpenSuse Leap | cmake, libopenssl-devel, zlib-devel
openSUSE Tumbleweed | clang, cmake, findutils, gcc-c++, libopenssl-devel, llvm-gold, make, zlib-devel
Raspbian 32-bit | cmake, libssl-dev, zlib1g-dev
Raspbian 64-bit | cmake, clang, libssl-dev, zlib1g-dev
Rocky | clang, cmake, diffutils, libatomic, libstdc++-static, make, openssl-devel, zlib-devel
Ubuntu | clang, cmake, libssl-dev, make, zlib1g-dev
Void | clang, cmake, make, libatomic, libatomic-devel, openssl-devel, zlib-devel

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### 32-bit Raspbian

32-bit ARM is a best-effort target — not tested in CI, but built and tested periodically on real hardware. A change can break it between those checks.

Installing on a 32-bit Raspbian is slightly different from other Linux based
Operating Systems. There are two important things to note:

- at the moment, only `gcc` can be used to build Pony; `clang` currently doesn't work.
- you'll need to override the CPU tuning (the presets default to `-mtune=generic`).

```bash
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake
cmake --preset release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_FLAGS="-march=native -mtune=native" -DCMAKE_CXX_FLAGS="-march=native -mtune=native"
cmake --build --preset release
sudo cmake --install build/build_release
```

### 64-bit Raspbian

Installing on a 64-bit Raspbian is slightly different from other Linux based
Operating Systems, you'll need to build for the `armv8-a` architecture, but otherwise, everything is the same.

```bash
cmake -DPONY_PIC_FLAG=-fPIC -P lib/build-libs.cmake
cmake --preset armv8-a-release -DPONY_PIC_FLAG=-fPIC
cmake --build --preset armv8-a-release
sudo cmake --install build/build_armv8-a-release
```

### Asahi

Installing on Asahi is slightly different due to running on the M1 processor. You'll need to build for the `armv8` architecture, but otherwise, everything is the same.

```bash
cmake -P lib/build-libs.cmake
cmake --preset armv8-release
cmake --build --preset armv8-release
sudo cmake --install build/build_armv8-release
```

## macOS

```bash
cmake -P lib/build-libs.cmake
cmake --preset release
cmake --build --preset release
sudo cmake --install build/build_release
```

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## Haiku

You'll need additional, non-default software packages on Haiku:

- cmake
- python3.14
- libexecinfo_devel
- openssl3_devel

A quick way to install those is to run following command in the Terminal:

```bash
pkgman install cmake python3.14 libexecinfo_devel openssl3_devel
```

Once that's done, rest of the steps are similar to other operating systems:

```bash
cmake -DPRESET=libs-haiku-x86-64 -P lib/build-libs.cmake
cmake --preset release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset release
cmake --install build/build_release --prefix /boot/system/non-packaged/
```

Note that you only need to run `cmake -DPRESET=libs-haiku-x86-64 -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

## Windows

Building on Windows requires the following:

- [CMake](https://cmake.org/download/) version 3.25 or higher needs to be in your PATH.
- [Python 3](https://www.python.org/downloads/)
- Visual Studio 2022 or 2019 (available [here](https://www.visualstudio.com/vs/community/)) or the Visual C++ Build Tools 2022 or 2019 (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - If using Visual Studio, install the `Desktop Development with C++` workload.
  - If using Visual C++ Build Tools, install the `Visual C++ build tools` workload, and the `Visual Studio C++ core features` individual component.
  - Install the `C++ Clang Compiler for Windows` component (under Individual Components). The build uses clang-cl, not MSVC's cl.exe, so that LTO-compiled libraries are compatible with the LLVM linker ponyc uses internally.
  - Install the latest `Windows 11 SDK (11.x.x.x) for Desktop` component.
- An SSL library built for MSVC.
- Windows long paths must be enabled. The LLVM build tree exceeds the 260-character `MAX_PATH` limit. In an admin PowerShell prompt, run `reg add "HKLM\SYSTEM\CurrentControlSet\Control\FileSystem" /v LongPathsEnabled /t REG_DWORD /d 1 /f` and restart your terminal.

In a PowerShell prompt, run:

```powershell
cmake -DPRESET=libs-windows-x86-64 -P lib/build-libs.cmake
cmake --preset windows-x86-64
cmake --build --preset windows-x86-64-release
```

The Visual Studio generator is multi-config, so one `cmake --preset windows-x86-64` configures the build and you pick debug or release at build time: `cmake --build --preset windows-x86-64-debug` or `windows-x86-64-release`.

Following building, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by running this in PowerShell: `[Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path', 'User') + ';<ponyc repo>\build\release', 'User')`

Note that you only need to run `cmake -P lib/build-libs.cmake` once the first time you build (or if the version of LLVM in the `lib/llvm/src` Git submodule changes).

### ARM64 Windows

On an ARM64 host, use the `windows-arm64` presets and `-DPRESET=libs-windows-arm64`.

```powershell
cmake -DPRESET=libs-windows-arm64 -P lib/build-libs.cmake
cmake --preset windows-arm64
cmake --build --preset windows-arm64-release
```

### Unsupported Windows build options

Several `use=` build options aren't supported on Windows. The supported ones are `systematic_testing`, `runtimestats`, and `runtimestats_messages`. The rest depend on POSIX interfaces not available on Windows: `pool_memalign` needs `posix_memalign`, the sanitizers need `-fsanitize=` runtimes that aren't shipped for Windows, and `coverage` needs `-fprofile-arcs` support. `cmake --preset windows-x86-64 -DPONY_USES=...` rejects the unsupported options with a clear error rather than failing partway through the build.

---

## The Runtime Allocator

The runtime's default allocator on every platform is the arena allocator designed in [discussion #5735](https://github.com/ponylang/ponyc/discussions/5735): it gets memory from the operating system in large shared regions, carves them into thread-owned arenas, reuses freed memory across threads and size classes, and gives an empty arena's physical pages back to the operating system.

The arena allocator carries no AddressSanitizer or Valgrind instrumentation, so combining it with `address_sanitizer` or `valgrind` is rejected at compile time: a clean run that checked nothing misleads. Pair those options with `pool_memalign`, which routes allocations through `posix_memalign`/`free` where the tools can track them.

One sizing limit to know about the arena allocator: memory comes from the operating system in 256 MiB regions (64 MiB on 32-bit platforms) whose address space is kept for the life of the process — emptied regions give back their physical pages but stay mapped for reuse. On Unix, reserving a region briefly maps twice its size before trimming, so a process running near an address-space cap needs twice the region size in headroom whenever the allocator opens a new region. Windows reserves a region at its alignment in one call and needs no such headroom.

---

## Additional Build Options on Unix

### arch

You can specify the CPU architecture to build Pony for. For the common architectures there are presets (`x86-64`, `armv8`, `armv8-a`, each with a `-debug`/`-release` variant); for anything else, set `PONY_ARCH` and the matching compiler flags directly:

```bash
cmake --preset release -DPONY_ARCH=arm7 -DCMAKE_C_FLAGS="-march=arm7 -mtune=generic" -DCMAKE_CXX_FLAGS="-march=arm7 -mtune=generic"
cmake --build --preset release
```

### sanitizers

ponyc can be built with the Clang/LLVM sanitizers to catch bugs in the compiler and the Pony runtime at runtime. Three `use=` options are available: `address_sanitizer` (AddressSanitizer — buffer overflows, use-after-free, double-free), `thread_sanitizer` (ThreadSanitizer — data races), and `undefined_behavior_sanitizer` (UndefinedBehaviorSanitizer — signed integer overflow, misaligned access, and other undefined behavior). AddressSanitizer and ThreadSanitizer can't be combined; either can be combined with UndefinedBehaviorSanitizer.

Pair `address_sanitizer` with `pool_memalign` so AddressSanitizer can track the Pony runtime's own allocations: `pool_memalign` routes every runtime allocation through `posix_memalign`/`free`, which AddressSanitizer intercepts and surrounds with redzones. The pairing is required, not just recommended — the default arena allocator rejects `address_sanitizer` at compile time (see the runtime allocator section above). The combination CI builds and tests has a preset:

```bash
cmake --preset debug-asan
cmake --build --preset debug-asan
```

The build lands in a suffixed directory — `build/debug-address_sanitizer-undefined_behavior_sanitizer-pool_memalign` for the configuration above. The suffix lists the enabled options in a fixed order set by CMake, independent of the order they were requested. The `debug-asan` preset builds in debug; the sanitizers build under release too (`release-asan`), and CI tests both.

The instrumented `ponyc` runs both during its own build (it compiles the self-hosted tools) and every time you use it to compile a program, so AddressSanitizer's runtime options must be set in the environment for `cmake --build` and for any later `ponyc` invocation:

- ponyc isn't LeakSanitizer-clean, so `detect_leaks=0` suppresses the compiler's own leak reports at exit.
- On platforms whose base system C++ runtime is libc++ (FreeBSD, macOS), the instrumented compiler shares `std::vector`/`std::string` with the non-instrumented vendored LLVM, and libc++'s container annotations then abort on false-positive container overflows; `detect_container_overflow=0` suppresses them. Linux uses libstdc++ and doesn't need it.

Platform | `ASAN_OPTIONS`
--- | ---
Linux | `detect_leaks=0`
FreeBSD, macOS | `detect_leaks=0:detect_container_overflow=0`

Both options concern `ponyc` itself, not the programs it compiles. A correct Pony program is LeakSanitizer-clean at exit, so run a program you compiled with LeakSanitizer left on — it will report genuine leaks in your own code, such as memory you allocate through FFI and never free. The container-overflow false positive is likewise specific to the compiler's libc++/LLVM sharing; a compiled program links the C runtime, not libc++ or LLVM, so it can't occur there.

For example, on Linux:

```bash
ASAN_OPTIONS=detect_leaks=0 cmake --build --preset debug-asan
ASAN_OPTIONS=detect_leaks=0 ./build/debug-address_sanitizer-undefined_behavior_sanitizer-pool_memalign/ponyc path/to/your/program
```

The sanitizers can't be built on OpenBSD or DragonFly BSD; see [Unsupported OpenBSD build options](#unsupported-openbsd-build-options) and [Unsupported DragonFly BSD build options](#unsupported-dragonfly-bsd-build-options).

### lto

Link-time optimization (LTO) is enabled automatically on all 64-bit release presets. On 32-bit builds, LTO is silently skipped to avoid excessive memory use during linking.

Caveats:

- If you aren't using clang as your linker, we've seen LTO generate incorrect binaries. It's rare but it can happen.

- If you are on macOS, turning on LTO means that if you upgrade your version of Xcode, you will have to rebuild your Pony compiler. You won't be able to link Pony programs if there is a mismatch between the version of Xcode used to build the Pony runtime and the version of Xcode you currently have installed.

To disable LTO for a release build, override the variable on the configure line:

```bash
cmake --preset release -DPONY_USE_LTO=OFF
cmake --build --preset release
```

### runtime-bitcode

On clang builds, `PONY_RUNTIME_BITCODE` defaults to `ON`. The build compiles every libponyrt source file to LLVM bitcode and links them into `libponyrt.bc` with `llvm-link` (built by `cmake -P lib/build-libs.cmake`). At compile time, ponyc looks for `libponyrt.bc` in its search paths and merges it into the program's IR before optimisation, enabling interprocedural optimisation between Pony code and the runtime. When `libponyrt.bc` is not found (e.g. on Windows builds), ponyc links `libponyrt.a` as usual.

To disable runtime bitcode on a clang build:

```bash
cmake --preset release -DPONY_RUNTIME_BITCODE=OFF
cmake --build --preset release
```

### systematic testing

Systematic testing allows for running of Pony programs in a deterministic manner. It accomplishes this by coordinating the interleaving of the multiple runtime scheduler threads in a deterministic and reproducible manner instead of allowing them all to run in parallel like happens normally. This ability to reproduce a particular runtime behavior is invaluable for debugging runtime issues.

Systematic testing is enabled by setting `use=systematic_testing` in the configure step like:

```bash
cmake --preset release -DPONY_USES=systematic_testing
cmake --build --preset release
```

More information about systematic testing can be found in [SYSTEMATIC_TESTING.md](SYSTEMATIC_TESTING.md).

### reach pass instrumentation

Emit reach-pass counters (reachable type count, per-scan candidate counts, subtype-check counts, cap-variant fan-out counts) to stderr at the end of each compile. For working on the reach pass — measuring where its time goes and confirming that a change moved the counts.

```bash
cmake --preset release -DPONY_USES=reach_instrument
cmake --build --preset release
```

## Compiler Development

To ease development and support LSP tools like [clangd](https://clangd.llvm.org), create a `compile_commands.json` file with the following steps:

1. Run the configure step with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`: `cmake --preset debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
2. symlink the generated `compile_commands.json` files into the project root directory:

  ```bash
  ln -sf build/build_debug/compile_commands.json compile_commands.json
  ```

  Replace `build_debug` with `build_release` if you are using a release configuration for compilation.

Now [clangd](https://clangd.llvm.org) will pick up the generated file and will be able to respond much quicker than without `compile_commands.json` file.
