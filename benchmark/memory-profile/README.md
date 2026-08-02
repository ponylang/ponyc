# Memory-profile benchmarks

Standalone Pony programs that exercise the arena allocator so its
`--ponymemoryprofile` dial (1 = smallest footprint, 10 = most throughput) can be
measured, and so its per-rung values can be re-derived later. Unlike the C++
Google Benchmark suites under `benchmark/libponyrt/`, these are ordinary Pony
programs: build them with the compiler you are testing and run them directly.

The dial only moves a program that frees memory and reuses it after a gap. A
program that reuses its working set in place is unaffected (it is the baseline),
so `churn` and `actor-churn` each take an `ngap` knob to set that gap;
`mixed-churn`'s gap is the ring itself, a freed burst coming back around after
the other workers' cycles.

## Programs

- `churn/` -- array churn. Each worker rebuilds a working set of large Pony
  arrays each round. `ngap` = 1 reuses the same size in place (baseline);
  `ngap` > 1 cycles through that many size classes, so a freed block is reused
  only after a gap.
- `actor-churn/` -- actor churn. Each round spawns a batch of short-lived worker
  actors that each allocate, touch, and die, so the arena churns through actor
  teardown rather than a loop.
- `mixed-churn/` -- cache-overflow churn. A ring of eight workers, each freeing
  an incoming burst of another worker's blocks and keeping scratch blocks of its
  own resident, so every burst lands on a thread cache holding a mix of own and
  foreign blocks. What this program varies is the burst size against the
  cache's depth: the thread cache's byte budget was sized against it (its
  docstring has the geometries and a sweep recipe). Its `--size` reaches the
  block and oversized tiers, where the large-retention budget governs
  instead of the cache, and its churn-shape flags cover two-size, growth,
  pipeline, and one-shot geometries. Run it with
  `--ponymaxthreads 8 --ponynoscale` -- eight workers carry the work, and a
  measurement run gets no more scheduler threads than the program keeps busy.

## Building and running

Build with the ponyc you are testing, e.g.:

```
./build/release/ponyc benchmark/memory-profile/churn -o /tmp/bin -b churn
```

Throughput is the program's own timer: `churn` and `actor-churn` print it as
`ns=...`, `mixed-churn` as `BATCHES_PER_SEC ...`. Peak resident memory is read
from outside with `/usr/bin/time -v` (the "Maximum resident set size" line). Pin the scheduler to a fixed, kept-busy thread count with
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

To search values beyond the ten rungs, set the four arena knobs directly (via
the runtime's `_for_test` seams), bypassing the dial. `churn` and `actor-churn`
take them as four more positional args, `floor span thresh budget`;
`mixed-churn` takes them as named flags:

```
/tmp/bin/churn 8 2000 65536 8 32 0 1 4 65536         # return everything (leanest)
/tmp/bin/churn 8 2000 65536 8 32 128 512 512 2097152 # hold everything (fastest)
/tmp/bin/mixed-churn --batch=128 --scratch=32 --total=1500000 --budget=1048576
```

`span` and `threshold` are in arena units; the arena is 512 units (8 MiB) on
64-bit. `budget` is bytes of cache per size class -- sweeping it against
`mixed-churn`'s geometries is how the dial's budget column was sized. See
`src/libponyrt/mem/pool_arena.c` for what each knob does.

`mixed-churn` also takes `--retain`, the large-retention byte budget (bytes
of freed block-class and oversized memory a thread may keep committed).
Unlike the four knobs above, its keep-the-profile value is the flag being
absent rather than 0 -- an explicit `--retain=0` turns retention off,
releasing freed large memory immediately instead of keeping it committed
for reuse.

## Program arguments

- `churn`: `workers rounds base-size ngap depth [floor span thresh budget]`
- `actor-churn`: `batch rounds base-size ngap [floor span thresh budget]`
- `mixed-churn`: named flags -- `--workers --size --batch --scratch --tokens
  --total`, plus `--floor --span --thresh --budget` for the same four raw
  knobs, each independent, 0 keeping that knob's profile value, and
  `--retain` (absent keeps the profile's; 0 is a real value). `--size`
  reaches 64 MiB, so one program covers the class, block, and oversized
  tiers. Churn-shape flags: `--size2` (alternate batch sizes), `--grow`
  (scratch becomes a realloc-doubling ladder), `--grow-once` (one growth
  per worker, then all churn at the target), `--seed` (a one-shot cold
  block per worker), `--pipeline` (worker 0 allocates, worker 1 frees),
  and `--tail` (post-run VmRSS samples, Linux only) -- the program
  docstring has each flag's exact shape.

For `churn` and `actor-churn`, each argument is optional from the left and must
be a non-negative integer; the trailing `floor span thresh budget` are
all-or-nothing. A non-numeric argument, or a partial raw-knob list, is rejected
with a usage message and a non-zero exit, rather than run silently at the
default.
