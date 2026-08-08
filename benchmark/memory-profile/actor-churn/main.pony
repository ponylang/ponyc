"""
Allocator memory-profile benchmark: actor churn.

Each round spawns a batch of short-lived worker actors; each allocates a large
Pony array, touches it, and finishes, so the actor dies and its heap is freed.
The allocation size cycles across `ngap` rounds, so freed heap memory is reused
after a gap. This drives the arena allocator through actor lifecycle (birth and
death), not a loop -- a common Pony pattern, and a high-churn / tiny-live-set
shape where holding freed memory helps at low memory cost.

Run under the dial and watch throughput against peak RSS:

    for r in 1 2 3 4 5 6 7 8 9 10; do
      /usr/bin/time -v ./actor-churn 64 8000 65536 8 --ponymemoryprofile $r
    done

Positional args: batch rounds base-size ngap, each optional from the left. Four
more optional args -- floor span thresh budget -- set the arena knobs directly
(bypassing the dial) for re-tuning sweeps, all four or none; omit them to honor
--ponymemoryprofile. A non-numeric argument, or a partial raw-knob list, is
rejected rather than silently run at the default.
"""
use "time"

use @ponyint_pool_arena_set_cache_floor_for_test[None](floor: USize)
use @ponyint_pool_arena_set_decommit_span_for_test[None](span: USize)
use @ponyint_pool_arena_set_dirty_threshold_for_test[None](threshold: USize)
use @ponyint_pool_arena_set_cache_budget_for_test[None](budget: USize)

actor Worker
  new create(coord: Coordinator, size: USize, seed: U8) =>
    let a = Array[U8].init(seed, size)
    coord.done(a.size())

actor Coordinator
  let _env: Env
  let _batch: USize
  let _rounds: USize
  let _sizes: Array[USize] val
  var _round: USize = 0
  var _pending: USize = 0
  var _checksum: USize = 0
  let _start: U64

  new create(env: Env, batch: USize, rounds: USize, sizes: Array[USize] val) =>
    _env = env
    _batch = batch
    _rounds = rounds
    _sizes = sizes
    _start = Time.nanos()
    _spawn()

  fun ref _spawn() =>
    let size = try _sizes(_round % _sizes.size())? else 262144 end
    _pending = _batch
    var k: USize = 0
    while k < _batch do
      Worker(this, size, (_round and 0xff).u8())
      k = k + 1
    end

  be done(s: USize) =>
    _checksum = _checksum + s
    _pending = _pending - 1
    if _pending == 0 then
      _round = _round + 1
      if _round < _rounds then
        _spawn()
      else
        let ns = Time.nanos() - _start
        _env.out.print("actor-churn ns=" + ns.string()
          + " cs=" + _checksum.string())
      end
    end

actor Main
  new create(env: Env) =>
    // A missing argument takes its default; a present but non-numeric one, or a
    // partial floor/span/thresh list, is a typo, so reject the run rather than
    // silently measure the default.
    try
      _run(env)?
    else
      env.err.print("usage: actor-churn [batch [rounds [base-size [ngap "
        + "[floor span thresh]]]]] -- each argument is a non-negative integer, "
        + "and floor/span/thresh are all-or-nothing")
      env.exitcode(1)
    end

  fun _run(env: Env) ? =>
    let batch = _arg(env, 1, 64)?
    let rounds = _arg(env, 2, 8000)?
    let base = _arg(env, 3, 65536)?
    let ngap = _arg(env, 4, 8)?

    // Optional raw-knob override (floor span thresh budget) for re-tuning
    // sweeps: all four or none. Read all four before setting any, so a partial
    // or garbled list errors out above and sets nothing. When omitted, the run
    // honors --ponymemoryprofile.
    if env.args.size() > 5 then
      let floor = env.args(5)?.usize()?
      let span = env.args(6)?.usize()?
      let thresh = env.args(7)?.usize()?
      let budget = env.args(8)?.usize()?
      @ponyint_pool_arena_set_cache_floor_for_test(floor)
      @ponyint_pool_arena_set_decommit_span_for_test(span)
      @ponyint_pool_arena_set_dirty_threshold_for_test(thresh)
      @ponyint_pool_arena_set_cache_budget_for_test(budget)
    end

    let sizes = recover val
      let arr = Array[USize](ngap)
      var j: USize = 0
      while j < ngap do arr.push(base * (j + 1)); j = j + 1 end
      arr
    end
    Coordinator(env, batch, rounds, sizes)

  fun _arg(env: Env, i: USize, fallback: USize): USize ? =>
    // Absent -> fallback. Present -> parse, erroring to _run's caller on a
    // non-numeric value, so a mistyped argument is rejected instead of run
    // silently at the default.
    if i >= env.args.size() then fallback else env.args(i)?.usize()? end
