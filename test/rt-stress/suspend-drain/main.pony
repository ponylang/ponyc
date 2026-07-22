"""
Suspend-and-drain stress engine.

Drives the allocator's suspend-and-drain path with real schedulers: memory
allocated on every scheduler thread is freed after those threads have
suspended, and the freeing must bring the physical memory back without
activating any of them into scheduling work.

Phases:

1. Allocate: N actors, spread across all scheduler threads, each
   allocate `--blocks` raw pool blocks of `--block-size` bytes (the
   default is an immediate-decommit span in the arena allocator, so its
   return is deterministic) and hand the pointers to one collector.
2. Scale down: the allocators go idle; the schedulers scale down. The
   collector polls `pony_active_schedulers` until the count reaches
   `--min-schedulers`.
3. Drain: the collector frees every raw block from its own thread —
   all of them foreign, owned by the suspended threads the allocators
   ran on. No actor work is involved, so with the collector's own busy
   polling keeping the runtime from reclaiming through any global path,
   the only thing that can bring the memory back is each owner draining
   its own inbox on its polling tick. The collector polls resident
   memory (VmRSS) until at least `--reclaim-fraction` of the payload
   has returned and the active count has held at the floor for a run of
   polls; a rise that persists with no work to run means draining
   activated a suspended thread into scheduling, which it must not.
4. Scale up: fresh work is spawned; the count must rise above the
   minimum, proving the suspended threads still activate after their
   drain episodes.

Exit code 0 with a report on success; 1 with the failed phase on failure.
Run with --ponyminthreads 1 so phase 2 has a floor to reach, and
--min-schedulers 2: polling rides a timer, every timer fire activates a
scheduler to run the poll, and so a floor of 1 is never reached.
"""
use "cli"
use "time"

use @pony_active_schedulers[U32]()
use @ponyint_pool_alloc_size[Pointer[U8]](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: USize)
use @memset[Pointer[None]](p: Pointer[U8] tag, c: I32, n: USize)
use @fopen[Pointer[None]](path: Pointer[U8] tag, mode: Pointer[U8] tag)
use @fgets[Pointer[U8]](buf: Pointer[U8] tag, n: I32, f: Pointer[None])
use @fclose[I32](f: Pointer[None])

actor Allocator
  """
  Allocates raw pool blocks — no Pony objects, no distributed GC — so
  the thread this constructor happens to run on is the blocks' owner,
  and freeing them later involves no actor work at all: pointer in,
  pointer out, and the only machinery between the free and the reclaim
  is the allocator's own delivery and drain path.
  """
  new create(collector: Collector, blocks: USize, block_size: USize) =>
    var i: USize = 0

    while i < blocks do
      let p = @ponyint_pool_alloc_size(block_size)
      @memset(p, 0xA5, block_size) // make the pages resident
      collector.collect(p.usize())
      i = i + 1
    end

    collector.allocator_done()

actor Collector
  let _env: Env
  var _blocks: Array[USize] = _blocks.create()
  let _expected_blocks: USize
  let _block_size: USize
  var _allocator_count: USize = 0
  let _payload_bytes: USize
  let _min_schedulers: U32
  let _reclaim_fraction: F64
  var _allocators_done: USize = 0
  var _polls: USize = 0
  var _rss_before_kb: USize = 0
  var _phase: USize = 1
  var _high_ms: U64 = 0
  var _settled_ms: U64 = 0
  var _released: Bool = false
  let _timers: Timers = Timers

  new create(env: Env, allocator_count: USize, blocks: USize,
    block_size: USize, min_schedulers: U32, reclaim_fraction: F64)
  =>
    _env = env
    _expected_blocks = allocator_count * blocks
    _block_size = block_size
    _payload_bytes = _expected_blocks * block_size
    _min_schedulers = min_schedulers
    _reclaim_fraction = reclaim_fraction

    _allocator_count = allocator_count

    var i: USize = 0

    while i < allocator_count do
      Allocator(this, blocks, block_size)
      i = i + 1
    end

  be collect(block: USize) =>
    _blocks.push(block)

  be allocator_done() =>
    _allocators_done = _allocators_done + 1

    if _allocators_done == _allocator_count then
      _polls = 0
      _phase = 2
      _schedule_poll()
    end

  fun ref _schedule_poll() =>
    """
    Polling rides a timer so nothing is runnable between polls: an
    always-runnable poll actor gets stolen back and forth, and every
    successful steal resets the thief's suspend clock — the schedulers
    would never scale down.
    """
    let c: Collector tag = this
    _timers(Timer(object iso is TimerNotify
      fun apply(timer: Timer, count: U64): Bool =>
        c.poll()
        false
    end, 250_000_000))

  be poll() =>
    match _phase
    | 2 => wait_for_scale_down()
    | 4 => wait_for_scale_up()
    end

  be poll_busy() =>
    """
    Phase 3's poll never sleeps: while this actor burns, at least one
    scheduler stays runnable, so the runtime never winds down and
    reclaims the memory through a global teardown that would mask the
    path under test. With one runnable actor and no other work, the only
    thing that reclaims a suspended owner's memory is that owner draining
    its own inbox on its polling tick: exactly what the phase asserts.
    """
    if _phase != 3 then
      return
    end

    var acc: U64 = 88172645463325252
    var i: U64 = 0

    while i < 2_000_000 do
      acc = (acc * 6364136223846793005) + 1442695040888963407
      i = i + 1
    end

    if acc != 0 then
      wait_for_reclaim()
    end

  fun ref wait_for_scale_down() =>
    if @pony_active_schedulers() <= _min_schedulers then
      _env.out.print("phase 2: scaled down to " +
        @pony_active_schedulers().string() + " schedulers after " +
        _polls.string() + " polls")
      _rss_before_kb = _read_rss_kb()
      _polls = 0
      _phase = 3
      _released = false
      poll_busy()
    else
      _polls = _polls + 1

      if _polls > 400 then
        _fail("phase 2: schedulers never scaled down to " +
          _min_schedulers.string() + " (at " +
          @pony_active_schedulers().string() + ")")
      else
        _schedule_poll()
      end
    end

  fun ref wait_for_reclaim() =>
    if not _released then
      // Free every block from this thread: all of them foreign to it,
      // owned by whichever threads the allocators ran on — threads that
      // are suspended now. No actor is involved; the deliveries and
      // the owners' tick-drains are the only machinery that runs.
      _released = true

      for ptr in _blocks.values() do
        @ponyint_pool_free_size(_block_size, ptr)
      end

      _blocks = _blocks.create()
      poll_busy()
      return
    end

    // The frees went to suspended owners; their tick-drains bring the
    // resident memory back. The runtime still has legitimate reasons
    // to activate a scheduler briefly even here — the cycle detector's
    // periodic prod makes an actor runnable — so a single raised
    // sample proves nothing. Persistence does: a scheduler activated by
    // real work re-suspends once the work is gone, so a count that stays
    // raised means something activated a thread and nothing brings it
    // back down. The assertion is the settled state: the payload
    // reclaimed and the count holding at the floor for a full second of
    // polls.
    let now = Time.millis()
    let active = @pony_active_schedulers()

    if active > _min_schedulers then
      if _high_ms == 0 then
        _high_ms = now
      elseif (now - _high_ms) > 2_000 then
        _fail("phase 3: active schedulers held at " + active.string() +
          " for 2s with one runnable actor; draining must never " +
          "raise the count")
        return
      end
    else
      _high_ms = 0
    end

    let rss_now = _read_rss_kb()
    let reclaimed_kb =
      if _rss_before_kb > rss_now then _rss_before_kb - rss_now
      else 0 end
    let needed_kb =
      ((_payload_bytes.f64() * _reclaim_fraction) / 1024).usize()

    if (reclaimed_kb >= needed_kb) and (active <= _min_schedulers) then
      if _settled_ms == 0 then
        _settled_ms = now
      elseif (now - _settled_ms) > 1_000 then
        _env.out.print("phase 3: reclaimed " + reclaimed_kb.string() +
          " KiB from suspended owners (needed " + needed_kb.string() +
          "), schedulers held at " + active.string())
        _polls = 0
        _scale_up()
        return
      end
    else
      _settled_ms = 0
    end

    _polls = _polls + 1

    if (_polls % 2_000) == 0 then
      _env.out.print("phase 3 poll " + _polls.string() + ": rss " +
        rss_now.string() + " KiB (baseline " +
        _rss_before_kb.string() + "), active " + active.string())
    end

    if _polls > 20_000 then
      if reclaimed_kb >= needed_kb then
        _fail("phase 3: reclaimed, but the scheduler count never held " +
          "at the floor for a full second")
      else
        _fail("phase 3: only " + reclaimed_kb.string() + " of " +
          needed_kb.string() +
          " KiB reclaimed while the owners stayed suspended")
      end
    else
      poll_busy()
    end

  fun ref _scale_up() =>
    """
    Fresh parallel work: the suspended threads must come fully active
    after their drain episodes.
    """
    var i: USize = 0

    while i < 32 do
      Spinner
      i = i + 1
    end

    _phase = 4
    _schedule_poll()

  fun ref wait_for_scale_up() =>
    if @pony_active_schedulers() > _min_schedulers then
      _env.out.print("phase 4: scaled back up to " +
        @pony_active_schedulers().string() + " schedulers")
      _env.out.print("suspend-drain: ok")
    else
      _polls = _polls + 1

      if _polls > 400 then
        _fail("phase 4: schedulers never scaled back up; an activation " +
          "was lost")
      else
        _schedule_poll()
      end
    end

  fun ref _fail(msg: String) =>
    _env.out.print("suspend-drain: FAILED: " + msg)
    _env.exitcode(1)
    _phase = 0 // no further polls fire; the process drains and exits

  fun _read_rss_kb(): USize =>
    """
    VmRSS from /proc/self/status, by hand: proc files report a zero
    size, which trips buffered readers, so this walks fgets lines and
    parses the digits directly.
    """
    let f = @fopen("/proc/self/status".cstring(), "r".cstring())

    if f.is_null() then
      return 0
    end

    var kb: USize = 0
    let buf = Array[U8].init(0, 256)

    while not @fgets(buf.cpointer(), 256, f).is_null() do
      // "VmRSS:" prefix, bytewise, then the digits.
      if ((try buf(0)? else 0 end) == 'V') and
        ((try buf(1)? else 0 end) == 'm') and
        ((try buf(2)? else 0 end) == 'R') and
        ((try buf(3)? else 0 end) == 'S') and
        ((try buf(4)? else 0 end) == 'S') and
        ((try buf(5)? else 0 end) == ':')
      then
        var i: USize = 6
        var started = false

        try
          while i < buf.size() do
            let c = buf(i)?

            if (c >= '0') and (c <= '9') then
              started = true
              kb = (kb * 10) + (c - '0').usize()
            elseif started or (c == 0) then
              break
            end

            i = i + 1
          end
        end

        break
      end
    end

    @fclose(f)
    kb

actor Spinner
  """
  Briefly-busy parallel work for the scale-up phase.
  """
  var _n: U64 = 0

  new create() =>
    spin(2_000)

  be spin(rounds: U64) =>
    var i: U64 = 0
    var acc: U64 = _n

    while i < 1_000_000 do
      acc = (acc * 6364136223846793005) + 1442695040888963407
      i = i + 1
    end

    _n = acc

    if rounds > 0 then
      spin(rounds - 1)
    end

actor Main
  new create(env: Env) =>
    let cs =
      try
        CommandSpec.leaf("suspend-drain",
          "Suspend-and-drain stress engine", [
          OptionSpec.u64("allocators", "Allocating actors"
            where default' = 8)
          OptionSpec.u64("blocks", "Blocks per allocator"
            where default' = 32)
          OptionSpec.u64("block-size", "Bytes per block"
            where default' = 524288)
          OptionSpec.u64("min-schedulers",
            "The floor phase 2 must reach; pass --ponyminthreads with the "
            + "same value" where default' = 1)
          OptionSpec.f64("reclaim-fraction",
            "Payload fraction that must return in phase 3"
            where default' = 0.5)
        ], [])?
      else
        env.exitcode(1)
        return
      end

    let cmd =
      match CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      else
        env.exitcode(1)
        return
      end

    Collector(env,
      cmd.option("allocators").u64().usize(),
      cmd.option("blocks").u64().usize(),
      cmd.option("block-size").u64().usize(),
      cmd.option("min-schedulers").u64().u32(),
      cmd.option("reclaim-fraction").f64())
