"""
Cache-overflow benchmark for comparing allocator shedding policies.

A ring of Worker actors circulates batches of raw pool blocks. On each batch a
worker frees every incoming block (mostly another thread's memory: a burst of
foreign frees), allocates a fresh batch and sends it to the next worker, then
runs a scratch cycle -- allocate `scratch` blocks, touch them, free them --
that leaves that many of its own blocks in the thread cache. The scratch
blocks are resident when the next burst lands, so the cycle a cache must hold
is `batch + scratch` blocks: a cycle above the per-class cache cap overflows a
cache holding a mix of own and foreign blocks, where the overflow path either
passes the arriving block onward or keeps it; a cycle at or below the cap
stays in the cache and serves as the guard configuration. Sizes above 1 MiB
take the block-class and oversized paths, whose retention is governed by the
large-retention byte budget rather than the per-class cache.

Blocks are raw ponyint_pool_alloc_size/free_size allocations, not Pony
objects, so the pool receives every allocation and free directly with no
garbage collector in between. Every block is written on allocation and its
head touched on free, so the payload lines travel between cores. Each batch
message carries the size its blocks were allocated at, so the freeing worker
always frees with the true size.

Usage:
  mixed-churn [--workers=N] [--size=BYTES] [--batch=N] [--scratch=N]
    [--tokens=N] [--total=N] [--floor=N] [--span=N] [--thresh=N] [--budget=N]
    [--retain=BYTES] [--size2=BYTES] [--grow=N] [--grow-once=BYTES]
    [--seed=BYTES] [--pipeline]

The geometries the thread-cache byte budget was sized against, as
batch/scratch pairs at the default 4096-byte size: 16/8 (a 24-block cycle the
cache holds at every rung above the leanest), 128/32 (a 160-block cycle,
exactly the default cache depth), and 512/64 (a 576-block cycle, past the
cache's hard depth limit). Eight workers carry the work, so a measurement run
uses `--ponymaxthreads 8 --ponynoscale`. Sweep the geometries under the dial
and watch throughput against peak RSS:

  for r in 1 2 3 4 5 6 7 8 9 10; do
    /usr/bin/time -v /tmp/bin/mixed-churn --batch=128 --scratch=32 \\
      --total=1500000 --ponymaxthreads 8 --ponynoscale --ponymemoryprofile $r
  done

`--floor`, `--span`, `--thresh`, and `--budget` replace the active memory
profile's cache floor, immediate-decommit span, dirty-sweep threshold, and
cache byte budget through the arena allocator's test seams. Each knob is
independent, and 0 leaves that knob's profile value in place -- for the
leanest configuration use `--ponymemoryprofile=1` rather than zero overrides.
A class's cache depth is the larger of budget/size and floor, so `--floor`
binds only where it exceeds the class's byte share; `--budget` is what moves
the 4096-byte class this program churns. `span` and `thresh` are in arena
units; `budget` is bytes of cache per size class.

`--retain` replaces the large-retention byte budget (bytes of freed
block-class and oversized memory a thread may keep committed). Unlike the
four knobs above, its keep-the-profile value is the flag being absent, not 0:
an explicit `--retain=0` turns retention off, releasing freed large memory
immediately instead of keeping it committed for reuse.

Churn-shape flags, all off by default:

- `--size2` alternates the circulating batch between `--size` and this size,
  cycle by cycle, so two sizes share one budget. The scratch cycle stays at
  `--size`.
- `--grow` turns each scratch iteration into a realloc-doubling ladder:
  allocate at `--size`, double N times via ponyint_pool_realloc_size, free at
  the final size. At most 10 doublings, and the ladder must top out at or
  below the `--size` cap. Not combinable with `--grow-once`.
- `--grow-once` has every worker make one realloc call from `--size` to this
  target at the start of its first cycle, and every later allocation runs at
  the target. With an oversized-tier `--size` (above ~8 MiB on 64-bit)
  and a target in a larger power-of-two reservation, the single growth
  orphans one smaller-reservation
  mapping while all steady churn carries the target's
  reservation key; at smaller sizes the freed original is a block span or
  cached block, and no orphaned key exists. The one-shot runs inside the
  first `receive`, on whichever thread carries that worker's churn.
- `--seed` has every worker allocate, touch, and free one block of this size
  once, at the start of its first cycle -- a one-shot cold block, on the
  worker's churn thread.
- `--pipeline` needs exactly 2 workers and splits the batch roles: worker 0
  allocates and sends the circulating batch, worker 1 only frees it. The
  scratch cycle still runs on both -- pass `--scratch=0` for a pure split.
  The split is between actors; running with as many scheduler threads as
  workers is what keeps the two roles on different threads. Both workers
  tick the batch counter -- the free-only worker's empty return legs
  count -- so BATCHES_PER_SEC is not comparable between pipeline and
  normal runs at the same `--total`.

The seams only exist in arena builds, so a binary compiled from this source
does not link against a runtime built with the classic or memalign allocator.

Runs until `total` batches have been processed, then prints a header with the
effective knob values, then:

  BATCHES_PER_SEC <rate>

The elapsed time is measured from the first batch processed to the last.
"""
use "cli"
use "time"
use @ponyint_pool_alloc_size[Pointer[U8]](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: USize)
use @ponyint_pool_realloc_size[Pointer[U8]](
  old_size: USize, new_size: USize, p: USize)
use @ponyint_pool_arena_set_cache_floor_for_test[None](floor: USize)
use @ponyint_pool_arena_set_decommit_span_for_test[None](span: USize)
use @ponyint_pool_arena_set_dirty_threshold_for_test[None](threshold: USize)
use @ponyint_pool_arena_set_cache_budget_for_test[None](budget: USize)
use @ponyint_pool_arena_set_large_retain_for_test[None](bytes: USize)
use @ponyint_pool_arena_cache_floor_for_test[USize]()
use @ponyint_pool_arena_decommit_span_for_test[USize]()
use @ponyint_pool_arena_dirty_threshold_for_test[USize]()
use @ponyint_pool_arena_cache_budget_for_test[USize]()
use @ponyint_pool_arena_large_retain_for_test[USize]()
use @memset[Pointer[None]](p: USize, c: I32, n: USize)

primitive _Normal
primitive _AllocOnly
primitive _FreeOnly

type _Role is (_Normal | _AllocOnly | _FreeOnly)

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env
    try
      let cs =
        CommandSpec.leaf("mixed-churn",
          "Cache-overflow benchmark for allocator shedding policies",
          [
            OptionSpec.u64("workers",
              "Number of worker actors in the ring"
              where default' = 8)
            OptionSpec.u64("size",
              "Block size in bytes"
              where default' = 4096)
            OptionSpec.u64("batch",
              "Blocks per circulating batch"
              where default' = 128)
            OptionSpec.u64("scratch",
              "Own blocks a worker leaves in its cache each cycle"
              where default' = 32)
            OptionSpec.u64("tokens",
              "Circulating batches per worker"
              where default' = 2)
            OptionSpec.u64("total",
              "Total batches to process"
              where default' = 1_000_000)
            OptionSpec.u64("floor",
              "Per-class cache floor override; 0 keeps the profile's"
              where default' = 0)
            OptionSpec.u64("span",
              "Immediate-decommit span override, in arena units; 0 keeps"
              where default' = 0)
            OptionSpec.u64("thresh",
              "Dirty-sweep threshold override, in arena units; 0 keeps"
              where default' = 0)
            OptionSpec.u64("budget",
              "Per-class cache byte budget override; 0 keeps the profile's"
              where default' = 0)
            OptionSpec.u64("retain",
              "Large-retention byte budget override; absent keeps the"
              + " profile's, 0 forces retention off"
              where default' = U64.max_value())
            OptionSpec.u64("size2",
              "Alternate circulating batches between --size and this;"
              + " 0 = off"
              where default' = 0)
            OptionSpec.u64("grow",
              "Realloc doublings per scratch iteration; 0 = off"
              where default' = 0)
            OptionSpec.u64("grow-once",
              "One realloc per worker from --size to this target, then all"
              + " churn at the target; 0 = off"
              where default' = 0)
            OptionSpec.u64("seed",
              "One-shot cold block per worker, this many bytes; 0 = off"
              where default' = 0)
            OptionSpec.bool("pipeline",
              "Two workers: worker 0 allocates the batch, worker 1 frees"
              + " it; scratch still runs on both"
              where default' = false)
          ],
          [])?.>add_help()?
      let cmd =
        match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
        | let c: Command => c
        | let ch: CommandHelp =>
          ch.print_help(env.out)
          error
        | let se: SyntaxError =>
          env.out.print(se.string())
          error
        end

      // The --size tier cap: one bound covers the class, block, and
      // oversized tiers.
      let size_cap: USize = 64 * 1024 * 1024

      let workers = cmd.option("workers").u64().usize()
      let size = cmd.option("size").u64().usize()
      let batch = cmd.option("batch").u64().usize()
      let scratch = cmd.option("scratch").u64().usize()
      let tokens = cmd.option("tokens").u64().usize()
      let total = cmd.option("total").u64()
      let floor = cmd.option("floor").u64().usize()
      let span = cmd.option("span").u64().usize()
      let thresh = cmd.option("thresh").u64().usize()
      let budget = cmd.option("budget").u64().usize()
      let retain = cmd.option("retain").u64()
      let size2 = cmd.option("size2").u64().usize()
      let grow = cmd.option("grow").u64().usize()
      let grow_once = cmd.option("grow-once").u64().usize()
      let seed = cmd.option("seed").u64().usize()
      let pipeline = cmd.option("pipeline").bool()

      if workers < 2 then
        env.out.print("mixed-churn: --workers must be at least 2")
        error
      end
      if (size < 64) or (size > size_cap) then
        // Below 64 the head touch on free would write past the smallest
        // block.
        env.out.print(
          "mixed-churn: --size must be 64 to " + size_cap.string())
        error
      end
      if (size2 != 0) and ((size2 < 64) or (size2 > size_cap)) then
        env.out.print(
          "mixed-churn: --size2 must be 0, or 64 to " + size_cap.string())
        error
      end
      if (seed != 0) and ((seed < 64) or (seed > size_cap)) then
        env.out.print(
          "mixed-churn: --seed must be 0, or 64 to " + size_cap.string())
        error
      end
      if (batch == 0) or (tokens == 0) or (total == 0) then
        env.out.print(
          "mixed-churn: --batch, --tokens, and --total must be nonzero")
        error
      end
      if (grow > 0) and (grow_once > 0) then
        env.out.print("mixed-churn: --grow and --grow-once are exclusive")
        error
      end
      if (size2 > 0) and (grow_once > 0) then
        // grow-once moves all churn to the target size; size2 would then
        // alternate against the target, not --size as documented.
        env.out.print("mixed-churn: --size2 and --grow-once are exclusive")
        error
      end
      if (grow > 0) and (scratch == 0) then
        env.out.print(
          "mixed-churn: --grow shapes the scratch cycle; --scratch is 0")
        error
      end
      if (grow_once != 0) and
        ((grow_once <= size) or (grow_once > size_cap))
      then
        env.out.print(
          "mixed-churn: --grow-once must exceed --size, up to "
          + size_cap.string())
        error
      end
      if (grow > 0) and ((grow > 10) or (size > (size_cap >> grow))) then
        env.out.print(
          "mixed-churn: --grow allows at most 10 doublings, topping out"
          + " at or below " + size_cap.string())
        error
      end
      if pipeline and (workers != 2) then
        env.out.print("mixed-churn: --pipeline needs exactly 2 workers")
        error
      end

      // Before any worker exists: the knobs are process-global and read
      // unsynchronized by every freeing thread.
      if floor > 0 then
        @ponyint_pool_arena_set_cache_floor_for_test(floor)
      end
      if span > 0 then
        @ponyint_pool_arena_set_decommit_span_for_test(span)
      end
      if thresh > 0 then
        @ponyint_pool_arena_set_dirty_threshold_for_test(thresh)
      end
      if budget > 0 then
        @ponyint_pool_arena_set_cache_budget_for_test(budget)
      end
      if retain != U64.max_value() then
        if retain > USize.max_value().u64() then
          env.out.print("mixed-churn: --retain exceeds this platform's USize")
          error
        end
        @ponyint_pool_arena_set_large_retain_for_test(retain.usize())
      end

      // The header records the effective values, read back from the
      // allocator, so a run at profile defaults is distinguishable from
      // one that overrode them.
      _env.out.print("# workers " + workers.string() +
        ", size " + size.string() +
        ", batch " + batch.string() +
        ", scratch " + scratch.string() +
        ", tokens " + tokens.string() +
        ", total " + total.string() +
        ", floor " + @ponyint_pool_arena_cache_floor_for_test().string() +
        ", span " + @ponyint_pool_arena_decommit_span_for_test().string() +
        ", thresh " + @ponyint_pool_arena_dirty_threshold_for_test().string() +
        ", budget " + @ponyint_pool_arena_cache_budget_for_test().string() +
        ", retain " + @ponyint_pool_arena_large_retain_for_test().string() +
        ", size2 " + size2.string() +
        ", grow " + grow.string() +
        ", grow-once " + grow_once.string() +
        ", seed " + seed.string() +
        ", pipeline " + pipeline.string())

      let counter = Counter(this, total)

      let ring: Array[Worker] iso = recover ring.create(workers) end
      var i: USize = 0
      while i < workers do
        let role: _Role =
          if pipeline then
            if i == 0 then _AllocOnly else _FreeOnly end
          else
            _Normal
          end
        ring.push(
          Worker(counter, size, batch
            where scratch = scratch, size2 = size2, grow = grow,
              grow_once = grow_once, seed = seed, role = role))
        i = i + 1
      end
      let ring': Array[Worker] val = consume ring

      // Wire the ring without indexing: chain each worker to the one
      // built before it, then close the loop.
      var prev: (Worker | None) = None
      var first: (Worker | None) = None
      for w in ring'.values() do
        match prev
        | let p: Worker => p.set_next(w)
        | None => first = w
        end
        prev = w
      end
      match (prev, first)
      | (let last: Worker, let head: Worker) => last.set_next(head)
      end
      counter.set_workers(ring')

      for w in ring'.values() do
        var t: USize = 0
        while t < tokens do
          w.receive(size, recover val Array[USize](0) end)
          t = t + 1
        end
      end
    else
      env.exitcode(1)
    end

  be finished(processed: U64, elapsed_ns: U64) =>
    """
    Prints the batch count, the elapsed time, and the throughput. The
    ring drains after this: stopped workers free their incoming batches
    without sending new ones.
    """
    let elapsed_s: F64 = elapsed_ns.f64() / 1_000_000_000
    let rate: F64 = processed.f64() / elapsed_s

    _env.out.print("total batches: " + processed.string())
    _env.out.print("elapsed ns: " + elapsed_ns.string())
    _env.out.print("BATCHES_PER_SEC " + rate.string())

actor Counter
  """
  Counts processed batches across the ring and measures elapsed time
  from the first to the last. Tells every worker to stop once the total
  is reached, so the circulating tokens die and the program can exit.
  """
  let _main: Main
  let _total: U64
  var _workers: Array[Worker] val = recover Array[Worker](0) end
  var _processed: U64 = 0
  var _start_ns: U64 = 0

  new create(main: Main, total: U64) =>
    _main = main
    _total = total

  be set_workers(workers: Array[Worker] val) =>
    _workers = workers

  be tick() =>
    _processed = _processed + 1
    if _processed == 1 then
      _start_ns = Time.nanos()
    end

    if _processed == _total then
      let elapsed_ns = Time.nanos() - _start_ns
      for w in _workers.values() do
        w.stop()
      end
      _main.finished(_processed, elapsed_ns)
    end

actor Worker
  """
  One ring member. Each cycle: free the incoming batch (a burst of
  frees, mostly of other threads' blocks) at the size the batch message
  carries, allocate and send a fresh batch to the next worker, then run
  the scratch cycle that leaves own blocks in the thread cache for the
  next burst to shed against. The role narrows this for --pipeline:
  an alloc-only worker skips the frees, a free-only worker sends empty
  batches.
  """
  let _counter: Counter
  var _size: USize
  let _batch: USize
  let _scratch: USize
  let _size2: USize
  let _grow: USize
  let _grow_once: USize
  let _seed: USize
  let _role: _Role
  let _scratch_held: Array[USize]
  var _use2: Bool = false
  var _next: (Worker | None) = None
  var _stopped: Bool = false
  var _initialized: Bool = false

  new create(counter: Counter, size: USize, batch: USize, scratch: USize,
    size2: USize, grow: USize, grow_once: USize, seed: USize, role: _Role)
  =>
    _counter = counter
    _size = size
    _batch = batch
    _scratch = scratch
    _size2 = size2
    _grow = grow
    _grow_once = grow_once
    _seed = seed
    _role = role
    _scratch_held = Array[USize](scratch)

  be set_next(w: Worker) =>
    _next = w

  be stop() =>
    _stopped = true

  fun ref _first_cycle_setup() =>
    // Runs at the start of the first receive, so the one-shots land on the
    // thread carrying this worker's churn. An actor constructor runs
    // synchronously in its creator's context, which would put every
    // worker's one-shot on the creating thread instead.
    if _seed > 0 then
      // A one-shot cold block: touched, freed, never reused at this size.
      let p = @ponyint_pool_alloc_size(_seed)
      @memset(p.usize(), 0x5A, 64)
      @ponyint_pool_free_size(_seed, p.usize())
    end

    if _grow_once > 0 then
      // One growth: the freed original is the orphaned reservation; all
      // later churn runs at the target size.
      var p = @ponyint_pool_alloc_size(_size)
      @memset(p.usize(), 0x5A, 64)
      p = @ponyint_pool_realloc_size(_size, _grow_once, p.usize())
      @memset(p.usize(), 0x5A, 64)
      @ponyint_pool_free_size(_grow_once, p.usize())
      _size = _grow_once
    end

  be receive(size: USize, batch: Array[USize] val) =>
    if not _initialized then
      _initialized = true
      _first_cycle_setup()
    end

    match _role
    | _AllocOnly => None
    else
      for p in batch.values() do
        @memset(p, 0, 64)
        @ponyint_pool_free_size(size, p)
      end
    end

    if _stopped then
      return
    end

    let cur =
      if (_size2 > 0) and _use2 then _size2 else _size end
    if _size2 > 0 then
      _use2 = not _use2
    end

    let out: Array[USize] iso = recover out.create(_batch) end
    match _role
    | _FreeOnly => None
    else
      var i: USize = 0
      while i < _batch do
        let p = @ponyint_pool_alloc_size(cur)
        @memset(p.usize(), 0xA5, cur)
        out.push(p.usize())
        i = i + 1
      end
    end

    match _next
    | let w: Worker => w.receive(cur, consume out)
    end

    if _grow > 0 then
      // Each scratch iteration is a realloc-doubling ladder instead of a
      // held block.
      var i: USize = 0
      while i < _scratch do
        var sz = _size
        var p = @ponyint_pool_alloc_size(sz)
        @memset(p.usize(), 0x5A, 64)
        var g: USize = 0
        while g < _grow do
          let nsz = sz << 1
          p = @ponyint_pool_realloc_size(sz, nsz, p.usize())
          @memset(p.usize(), 0x5A, 64)
          sz = nsz
          g = g + 1
        end
        @ponyint_pool_free_size(sz, p.usize())
        i = i + 1
      end
    else
      var i: USize = 0
      while i < _scratch do
        let p = @ponyint_pool_alloc_size(_size)
        @memset(p.usize(), 0x5A, 64)
        _scratch_held.push(p.usize())
        i = i + 1
      end
      for p in _scratch_held.values() do
        @ponyint_pool_free_size(_size, p)
      end
      _scratch_held.clear()
    end

    _counter.tick()
