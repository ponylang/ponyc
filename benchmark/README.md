Benchmarks for the Pony runtime.

This directory holds benchmarks for the Pony runtime, built with the Google Benchmark library. Build and run the runtime benchmarks with:

```
cmake --build --preset release --target libponyrt.benchmarks
build/release/libponyrt.benchmarks
```

The compiler benchmarks use the `libponyc.benchmarks` target and run the same way. See `libponyrt/mem/README.md` for what the runtime's memory benchmarks measure.

Google Benchmark's own documentation and examples are at https://github.com/google/benchmark.
