"""
Allocator memory-profile benchmark: array churn.

Each worker rebuilds a working set of large Pony arrays every round. With
`ngap` = 1 the same size is reused in place every round (a recycle workload the
memory profile cannot help -- freed memory is handed straight back). With
`ngap` > 1 the size cycles through `ngap` distinct classes, so a freed block is
not reused until `ngap` rounds later (a build-and-discard workload): that gap is
what the --ponymemoryprofile dial trades against. Allocation uses ordinary Pony
`Array`s, so the pattern is realistic; one round per behavior so GC reclaims
between rounds.

Run under the dial and watch throughput against peak RSS:

    for r in 1 2 3 4 5 6 7 8 9 10; do
      /usr/bin/time -v ./churn 8 2000 65536 8 32 --ponymemoryprofile $r
    done

Positional args: workers rounds base-size ngap depth, each optional from the
left. Three more optional args -- floor span thresh -- set the arena knobs
directly (bypassing the dial) for re-tuning sweeps, all three or none; omit them
to honor --ponymemoryprofile. A non-numeric argument, or a partial
floor/span/thresh list, is rejected rather than silently run at the default.
"""
use "time"

use @ponyint_pool_arena_set_cache_floor_for_test[None](floor: USize)
use @ponyint_pool_arena_set_decommit_span_for_test[None](span: USize)
use @ponyint_pool_arena_set_dirty_threshold_for_test[None](threshold: USize)

actor Worker
  let _coord: Coordinator
  let _rounds: USize
  let _depth: USize
  let _sizes: Array[USize] val
  var _r: USize = 0
  var _checksum: USize = 0
  var _ws: Array[Array[U8]]

  new create(coord: Coordinator, rounds: USize, depth: USize,
    sizes: Array[USize] val)
  =>
    _coord = coord
    _rounds = rounds
    _depth = depth
    _sizes = sizes
    _ws = Array[Array[U8]](depth)
    step()

  be step() =>
    if _r >= _rounds then
      _coord.done(_checksum)
      return
    end
    let size = try _sizes(_r % _sizes.size())? else 262144 end
    _ws = Array[Array[U8]](_depth)  // drop last round's set -> garbage
    var i: USize = 0
    while i < _depth do
      let a = Array[U8].init((_r and 0xff).u8(), size)  // allocate + touch
      _checksum = _checksum + a.size()
      _ws.push(a)
      i = i + 1
    end
    _r = _r + 1
    step()

actor Coordinator
  let _env: Env
  let _workers: USize
  var _remaining: USize
  let _start: U64
  var _checksum: USize = 0

  new create(env: Env, workers: USize, rounds: USize, depth: USize,
    sizes: Array[USize] val)
  =>
    _env = env
    _workers = workers
    _remaining = workers
    _start = Time.nanos()
    var k: USize = 0
    while k < workers do
      Worker(this, rounds, depth, sizes)
      k = k + 1
    end

  be done(cs: USize) =>
    _checksum = _checksum + cs
    _remaining = _remaining - 1
    if _remaining == 0 then
      let ns = Time.nanos() - _start
      _env.out.print("churn ns=" + ns.string()
        + " workers=" + _workers.string() + " cs=" + _checksum.string())
    end

actor Main
  new create(env: Env) =>
    // A missing argument takes its default; a present but non-numeric one, or a
    // partial floor/span/thresh list, is a typo, so reject the run rather than
    // silently measure the default.
    try
      _run(env)?
    else
      env.err.print("usage: churn [workers [rounds [base-size [ngap [depth "
        + "[floor span thresh]]]]]] -- each argument is a non-negative integer, "
        + "and floor/span/thresh are all-or-nothing")
      env.exitcode(1)
    end

  fun _run(env: Env) ? =>
    let workers = _arg(env, 1, 8)?
    let rounds = _arg(env, 2, 2000)?
    let base = _arg(env, 3, 65536)?
    let ngap = _arg(env, 4, 8)?
    let depth = _arg(env, 5, 32)?

    // Optional raw-knob override (floor span thresh) for re-tuning sweeps: all
    // three or none. Read all three before setting any, so a partial or garbled
    // list errors out above and sets nothing. When omitted, the run honors
    // --ponymemoryprofile.
    if env.args.size() > 6 then
      let floor = env.args(6)?.usize()?
      let span = env.args(7)?.usize()?
      let thresh = env.args(8)?.usize()?
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
    Coordinator(env, workers, rounds, depth, sizes)

  fun _arg(env: Env, i: USize, fallback: USize): USize ? =>
    // Absent -> fallback. Present -> parse, erroring to _run's caller on a
    // non-numeric value, so a mistyped argument is rejected instead of run
    // silently at the default.
    if i >= env.args.size() then fallback else env.args(i)?.usize()? end
