Benchmarks for the Pony runtime.

Under this directory are benchmarks for the pony runtime and for libponyc
that use the Google Benchmark Library.

The benchmarks build as part of a normal ponyc build. BUILD.md has the
platform setup and the full steps; the build itself is:

```bash
cmake --preset release
cmake --build --preset release
```

That produces one executable per subdirectory here, next to ponyc in
`build/release`:

- `libponyrt.benchmarks`
- `libponyc.benchmarks`

Run either one directly. `--help` lists the Google Benchmark options,
among them `--benchmark_filter` to select cases by name and
`--benchmark_repetitions` to repeat them.

See `libponyrt/mem/README.md` for what the runtime's memory benchmarks measure.

The `memory-profile/` directory holds a different kind of benchmark: standalone Pony programs that exercise the arena allocator's `--ponymemoryprofile` dial, so the dial's per-rung values can be measured and re-derived. They are ordinary Pony programs, not Google Benchmark suites -- build them with the compiler you are testing and run them directly. See `memory-profile/README.md`.

`cmake --install` does not install the benchmark binaries.

`lib/CMakeLists.txt` sets the Google Benchmark version in
`PONYC_GBENCHMARK_URL`. Google Benchmark documents its options and its
API at https://github.com/google/benchmark.
