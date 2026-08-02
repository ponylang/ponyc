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
stays in the cache and serves as the guard configuration.

Blocks are raw ponyint_pool_alloc_size/free_size allocations, not Pony
objects, so the pool receives every allocation and free directly with no
garbage collector in between. Every block is written on allocation and its
head touched on free, so the payload lines travel between cores.

Usage:
  mixed-churn [--workers=N] [--size=BYTES] [--batch=N] [--scratch=N]
    [--tokens=N] [--total=N] [--floor=N] [--span=N] [--thresh=N] [--budget=N]

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
units; `budget` is bytes of cache per size class. The seams only exist in
arena builds, so a binary compiled from this source does not link against a
runtime built with the classic or memalign allocator.

Runs until `total` batches have been processed, then prints a header with the
effective knob values, then:

  BATCHES_PER_SEC <rate>

The elapsed time is measured from the first batch processed to the last.
"""
use "cli"
use "time"
use @ponyint_pool_alloc_size[Pointer[U8]](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: USize)
use @ponyint_pool_arena_set_cache_floor_for_test[None](floor: USize)
use @ponyint_pool_arena_set_decommit_span_for_test[None](span: USize)
use @ponyint_pool_arena_set_dirty_threshold_for_test[None](threshold: USize)
use @ponyint_pool_arena_set_cache_budget_for_test[None](budget: USize)
use @ponyint_pool_arena_cache_floor_for_test[USize]()
use @ponyint_pool_arena_decommit_span_for_test[USize]()
use @ponyint_pool_arena_dirty_threshold_for_test[USize]()
use @ponyint_pool_arena_cache_budget_for_test[USize]()
use @memset[Pointer[None]](p: USize, c: I32, n: USize)

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

      if workers < 2 then
        env.out.print("mixed-churn: --workers must be at least 2")
        error
      end
      if (size < 64) or (size > 1048576) then
        // Below 64 the head touch on free would write past the smallest
        // block; above 1 MiB the allocation takes the block path, which
        // has no thread cache to measure.
        env.out.print("mixed-churn: --size must be 64 to 1048576")
        error
      end
      if (batch == 0) or (tokens == 0) or (total == 0) then
        env.out.print(
          "mixed-churn: --batch, --tokens, and --total must be nonzero")
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
        ", budget " + @ponyint_pool_arena_cache_budget_for_test().string())

      let counter = Counter(this, total)

      let ring: Array[Worker] iso = recover ring.create(workers) end
      var i: USize = 0
      while i < workers do
        ring.push(Worker(counter, size, batch, scratch))
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
          w.receive(recover val Array[USize](0) end)
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
  frees, mostly of other threads' blocks), allocate and send a fresh
  batch to the next worker, then run the scratch cycle that leaves own
  blocks in the thread cache for the next burst to shed against.
  """
  let _counter: Counter
  let _size: USize
  let _batch: USize
  let _scratch: USize
  let _scratch_held: Array[USize]
  var _next: (Worker | None) = None
  var _stopped: Bool = false

  new create(counter: Counter, size: USize, batch: USize, scratch: USize) =>
    _counter = counter
    _size = size
    _batch = batch
    _scratch = scratch
    _scratch_held = Array[USize](scratch)

  be set_next(w: Worker) =>
    _next = w

  be stop() =>
    _stopped = true

  be receive(batch: Array[USize] val) =>
    for p in batch.values() do
      @memset(p, 0, 64)
      @ponyint_pool_free_size(_size, p)
    end

    if _stopped then
      return
    end

    let out: Array[USize] iso = recover out.create(_batch) end
    var i: USize = 0
    while i < _batch do
      let p = @ponyint_pool_alloc_size(_size)
      @memset(p.usize(), 0xA5, _size)
      out.push(p.usize())
      i = i + 1
    end

    match _next
    | let w: Worker => w.receive(consume out)
    end

    i = 0
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

    _counter.tick()
