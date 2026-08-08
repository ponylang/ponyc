Benchmarks for libponyrt's memory allocators.

The benchmarks in `pool.cc` measure the pool allocator; those in `heap.cc` measure the actor heap allocator. This README documents the `pool.cc` benchmarks.

## Building and running

Build the benchmark binary, then run it:

```
cmake --build --preset release --target libponyrt.benchmarks
build/release/libponyrt.benchmarks
```

Run a subset by name:

```
build/release/libponyrt.benchmarks --benchmark_filter=size_sweep_churn
```

Set the `PONY_MEMPROFILE` environment variable to a rung `1` through `10` (or the names `low`, `balanced`, or `throughput`, which are rungs 1, 3, and 10) to select the arena allocator's memory profile for the run — the same 1-to-10 dial the `--ponymemoryprofile` runtime option makes for a Pony program. The default is rung 3, and it affects only the arena backend.

## pool.cc

The pool allocator has two entry points. The `POOL_ALLOC`/`POOL_FREE` macros take a type and allocate its size class, fixed at compile time. `ponyint_pool_alloc_size`/`ponyint_pool_free_size` take a size at runtime. The allocator's design — arenas, slabs, units, and size classes — lives in `src/libponyrt/mem/pool_arena.c`; the descriptions below rely on it.

### Fixed-cost micro-benchmarks

`pool_index` times `ponyint_pool_index`, the size-to-size-class lookup, across sizes from 1 byte to 1 MB.

The rest time allocate and free, and their names encode three choices:

- Entry point. The `POOL_*` benchmarks use the compile-time macro on a fixed 32-byte block. The `pool_*_size` benchmarks use the runtime-size functions at a size one byte past the largest size class, which takes the large-allocation path.
- What is timed. A `$` names the single operation inside the timer, with its partner run outside it: `POOL_ALLOC$` times only the allocation, `POOL_FREE$` only the free. A benchmark with no `$` in its name times a full allocate-and-free cycle.
- Count. A `_multiple` benchmark allocates a batch per iteration instead of one — 1024 blocks for the fixed-size family, and for the large-allocation family `POOL_MMAP/LARGE_ALLOC` blocks, about 128 MB worth.

That gives, for the fixed 32-byte block, `POOL_ALLOC$`, `POOL_FREE$`, `POOL_ALLOC_FREE`, and their batched forms `POOL_ALLOC_multiple$_`, `POOL_FREE_multiple$_`, `POOL_ALLOC_FREE_multiple`; and the same six for the large-allocation path as `pool_alloc_size$`, `pool_free_size$`, `pool_alloc_free_size`, `pool_alloc_size_multiple$_`, `pool_free_size_multiple$_`, `pool_alloc_free_size_multiple`.

### Arena throughput benchmarks

These were added with the arena allocator. They cover the areas where the arena allocator differs from the classic pool allocator: same-thread churn across the size range and how it scales with the number of blocks held outstanding, the page return a long-lived workload drives, and cross-thread frees.

`size_sweep_churn` measures same-thread allocate-and-free throughput at a range of sizes. Each trial allocates two blocks at one size, frees them, and times both. Which path a size reaches depends on the per-class thread cache: it holds freed blocks per size class — up to the larger of the profile's byte budget divided by the class size and the profile's floor count — and returns them on the next allocation without touching the slab. At the default profile every class's cap is at least the floor of 8, so a working set of two stays in the cache at every class size and the class-sized rows measure the cache's pop and push; under `PONY_MEMPROFILE=1` (floor 0) the classes from 128 KB up hold no blocks, and the same rows drive slab reserve and release. 2 MB and 4 MB take the multi-unit block path inside an arena, and 16 MB takes its own mapping.

`size_sweep_depth` measures how that same-thread churn scales with the number of blocks held outstanding at once, at a fixed size — the depth `size_sweep_churn`'s working set of two does not reach. Each trial allocates `depth` blocks of one size, holds them all live, then frees them. The per-class cache absorbs only its first few; past it, a capacity-1 class (16 KB and up, one object per slab) has no partial-slab list to reuse, so every further allocation drives a slab reserve and every free a slab release. The 16 KB rows ramp the depth across the class's default-profile cap of 40 to show the crossover as the cache is outrun; 256 KB and 1 MB's deeper rows pass their floor cap of 8 almost at once. The 32 B row is the control — a class with many objects per slab bump-allocates within one slab and stays cheap at any depth. Set `PONY_MEMPROFILE` to measure it under each memory profile's per-class cache depth.

`hold_then_free` churns large same-thread blocks while holding a fixed base of them live. Before the timed loop it allocates `count` blocks and keeps them until the benchmark ends, so the arenas stay partly occupied. Each timed cycle then allocates `count` more blocks, writes one byte per page so every page is resident, holds them, and frees them. The `--ponymemoryprofile` option selects how much freed memory each thread holds for reuse and how quickly the allocator returns the rest to the OS; keeping the arenas partly occupied is what keeps these frees on that return path rather than the empty-arena path, which returns pages regardless of the setting. Returning pages quickly forces the next cycle to fault them back in — slower, but lower resident memory; holding them lets the next cycle reuse them — faster, but higher resident memory. This is the workload the presets' page-return settings were tuned against.

`cross_thread_free` runs two threads that each allocate their own blocks and free the other thread's. That drives the cross-thread free path on both sides: the freeing thread hands the block back to the thread that owns it, and the owner reclaims it on a later allocation. The classic pool allocator has no separate cross-thread path — a thread that frees another thread's block keeps it on its own free list — so this measures the two designs on the same workload rather than one internal path against another. Blocks are a fixed 32 bytes: cross-thread frees in Pony are message objects, which are small, and a single size keeps the between-trials cleanup of leftover blocks simple.
