Benchmarks for the Pony runtime.

This directory holds benchmarks for the Pony runtime, built with the Google Benchmark library. Build and run the runtime benchmarks with:

```
cmake --build --preset release --target libponyrt.benchmarks
build/release/libponyrt.benchmarks
```

The compiler benchmarks use the `libponyc.benchmarks` target and run the same way. See `libponyrt/mem/README.md` for what the runtime's memory benchmarks measure.

The `memory-profile/` directory holds a different kind of benchmark: standalone Pony programs that exercise the arena allocator's `--ponymemoryprofile` dial, so the dial's per-rung values can be measured and re-derived. They are ordinary Pony programs, not Google Benchmark suites -- build them with the compiler you are testing and run them directly. See `memory-profile/README.md`.

Google Benchmark's own documentation and examples are at https://github.com/google/benchmark.
