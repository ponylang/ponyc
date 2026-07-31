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

Positional args: batch rounds base-size ngap. Three more optional args -- floor
span thresh -- set the arena knobs directly (bypassing the dial) for re-tuning
sweeps; omit them to honor --ponymemoryprofile.
"""
use "time"

use @ponyint_pool_arena_set_cache_floor_for_test[None](floor: USize)
use @ponyint_pool_arena_set_decommit_span_for_test[None](span: USize)
use @ponyint_pool_arena_set_dirty_threshold_for_test[None](threshold: USize)

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
    let batch = try env.args(1)?.usize()? else 64 end
    let rounds = try env.args(2)?.usize()? else 8000 end
    let base = try env.args(3)?.usize()? else 65536 end
    let ngap = try env.args(4)?.usize()? else 8 end

    // Optional raw-knob override (floor span thresh) for re-tuning sweeps. Read
    // all three before setting any, so a partial list overrides nothing. When
    // omitted, the run honors --ponymemoryprofile.
    try
      let floor = env.args(5)?.usize()?
      let span = env.args(6)?.usize()?
      let thresh = env.args(7)?.usize()?
      @ponyint_pool_arena_set_cache_floor_for_test(floor)
      @ponyint_pool_arena_set_decommit_span_for_test(span)
      @ponyint_pool_arena_set_dirty_threshold_for_test(thresh)
    end

    let sizes = recover val
      let arr = Array[USize](ngap)
      var j: USize = 0
      while j < ngap do arr.push(base * (j + 1)); j = j + 1 end
      arr
    end
    Coordinator(env, batch, rounds, sizes)
