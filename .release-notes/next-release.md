## Update to LLVM 23.1.2

We've updated the LLVM version used to build Pony from 22.1.6 to 23.1.2.

## Runtime tracing flags available in all builds

The runtime tracing flags (`--tracecategories`, `--flightrecorder`, `--ponytracingmode`, etc.) are now available in every ponyc build. Previously they required building ponyc with the `runtime_tracing` option enabled. When no tracing flags are passed, the runtime checks a single boolean at each trace point and skips the call — benchmarks show the overhead is not measurable.

## Remove `runtime_tracing` build option

Runtime tracing is now always compiled in. The `runtime_tracing` entry in `PONY_USES` is no longer accepted — drop it from your CMake configuration.

The `pony_type_t` struct layout changed: `name` and `get_behavior_name` fields are now always present. Code that constructs `pony_type_t` values directly in C must be recompiled.

