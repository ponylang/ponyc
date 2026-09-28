## Update to LLVM 23.1.2

We've updated the LLVM version used to build Pony from 22.1.6 to 23.1.2.

## Runtime tracing flags available in all builds

The runtime tracing flags (`--tracecategories`, `--flightrecorder`, `--ponytracingmode`, etc.) are now available in every ponyc build. Previously they required building ponyc with the `runtime_tracing` option enabled. When no tracing flags are passed, the runtime checks a single boolean at each trace point and skips the call — benchmarks show the overhead is not measurable.

## Remove `runtime_tracing` build option

Runtime tracing is now always compiled in. The `runtime_tracing` entry in `PONY_USES` is no longer accepted — drop it from your CMake configuration.

The `pony_type_t` struct layout changed: `name` and `get_behavior_name` fields are now always present. Code that constructs `pony_type_t` values directly in C must be recompiled.

## Reject runtime-reserved signals in HandleableSignalValidator

`HandleableSignalValidator` now rejects signals the runtime reserves for its tracing thread-pause mechanism. On BSD and macOS, `Sig.info()` (SIGINFO) is rejected. On Linux, real-time signals 32 through 35 are rejected — the C library and the runtime reserve them.

Programs that registered a `SignalHandler` for one of these signals will now get a `ValidationFailure` from `MakeHandleableSignal` instead of silently replacing the runtime's handler.

## Fix ARM64 Windows LTO crash

Building ponyc from source on ARM64 Windows crashed during LTO linking with an access violation in LLVM's type legalizer. MSVC generates incorrect ARM64 machine code for functions in this pass. The vendored LLVM libraries are now built with clang-cl on this platform.

## Build vendored LLVM libraries with clang-cl on ARM64 Windows

The vendored LLVM libraries on ARM64 Windows are now built with clang-cl instead of MSVC. If you build ponyc from source on ARM64 Windows, install the "C++ Clang Compiler for Windows" component in your Visual Studio installation — it is not part of the default "Desktop Development with C++" workload.
## Fix compiling with runtime tracing on the BSDs

Compiling with runtime tracing on the BSDs failed because no one had ever tried before and noticed that it didn't work. FreeBSD, OpenBSD and Dragonfly have all been fixed.

