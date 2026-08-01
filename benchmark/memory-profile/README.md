# Memory-profile benchmarks

Standalone Pony programs that exercise the arena allocator so its
`--ponymemoryprofile` dial (1 = smallest footprint, 10 = most throughput) can be
measured, and so its per-rung values can be re-derived later. Unlike the C++
Google Benchmark suites under `benchmark/libponyrt/`, these are ordinary Pony
programs: build them with the compiler you are testing and run them directly.

The dial only moves a program that frees memory and reuses it after a gap. A
program that reuses its working set in place is unaffected (it is the baseline),
so each program takes an `ngap` knob to set that gap.

## Programs

- `churn/` -- array churn. Each worker rebuilds a working set of large Pony
  arrays each round. `ngap` = 1 reuses the same size in place (baseline);
  `ngap` > 1 cycles through that many size classes, so a freed block is reused
  only after a gap.
- `actor-churn/` -- actor churn. Each round spawns a batch of short-lived worker
  actors that each allocate, touch, and die, so the arena churns through actor
  teardown rather than a loop.

## Building and running

Build with the ponyc you are testing, e.g.:

```
./build/release/ponyc benchmark/memory-profile/churn -o /tmp/bin -b churn
```

Throughput is the program's own timer, printed as `ns=...`. Peak resident memory
is read from outside with `/usr/bin/time -v` (the "Maximum resident set size"
line). Pin the scheduler to a fixed, kept-busy thread count with
`--ponymaxthreads N --ponynoscale`.

### Dial sweep

Run the program across the ten rungs and compare throughput against peak RSS:

```
for r in 1 2 3 4 5 6 7 8 9 10; do
  /usr/bin/time -v /tmp/bin/churn 8 2000 65536 8 32 --ponymaxthreads 8 \
    --ponynoscale --ponymemoryprofile $r
done
```

### Raw-knob sweep (re-tuning)

To search values beyond the ten rungs, pass four more positional args,
`floor span thresh budget`, which set the arena knobs directly (via the
runtime's `_for_test` seams) and bypass the dial:

```
/tmp/bin/churn 8 2000 65536 8 32 0 1 4 65536         # return everything (leanest)
/tmp/bin/churn 8 2000 65536 8 32 128 512 512 2097152 # hold everything (fastest)
```

`span` and `threshold` are in arena units; the arena is 512 units (8 MiB) on
64-bit. `budget` is bytes of cache per size class. See
`src/libponyrt/mem/pool_arena.c` for what each knob does.

## Program arguments

- `churn`: `workers rounds base-size ngap depth [floor span thresh budget]`
- `actor-churn`: `batch rounds base-size ngap [floor span thresh budget]`

Each argument is optional from the left and must be a non-negative integer; the
trailing `floor span thresh budget` are all-or-nothing. A non-numeric argument, or a
partial raw-knob list, is rejected with a usage message and a non-zero exit,
rather than run silently at the default.
