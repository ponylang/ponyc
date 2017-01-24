use "collections"
use "promises"

actor _AutoBench[A: Any #share] is _Benchmark
  let _name: String
  let _notify: _BenchNotify
  let _f: {(): A ?} val
  var _bench: _Bench[A]
  let _auto_ops: _AutoOps

  new create(
    name: String,
    notify: _BenchNotify,
    f: {(): A ?} val,
    bench_time: U64 = 1_000_000_000,
    max_ops: U64 = 100_000_000
  ) =>
    _name = name
    _notify = notify
    _f = f
    _bench = _Bench[A](_name, this, _f, 1)
    _auto_ops = _AutoOps(bench_time, max_ops)

  be run() => _bench.run()

  be _result(name: String, ops: U64, nspo: U64) =>
    match _auto_ops(ops, ops*nspo, nspo)
    | let ops': U64 =>
      _bench = _Bench[A](_name, this, _f, ops')
      run()
    else _notify._result(name, ops, nspo)
    end

  be _failure(name: String, timeout: Bool) =>
    _notify._failure(name, timeout)

actor _AutoBenchAsync[A: Any #share] is _Benchmark
    let _name: String
    let _notify: _BenchNotify
    let _f: {(): Promise[A] ?} val
    let _timeout: U64
    var _bench: _BenchAsync[A]
    let _auto_ops: _AutoOps
  
    new create(
      name: String,
      notify: _BenchNotify,
      f: {(): Promise[A] ?} val,
      timeout: U64,
      bench_time: U64 = 1_000_000_000,
      max_ops: U64 = 100_000_000
    ) =>
      _name = name
      _notify = notify
      _f = f
      _timeout = timeout
      _bench = _BenchAsync[A](_name, this, _f, 1, _timeout)
      _auto_ops = _AutoOps(bench_time, max_ops)
  
    be run() => _bench.run()
  
    be _result(name: String, ops: U64, nspo: U64) =>
      match _auto_ops(ops, ops*nspo, nspo)
      | let ops': U64 =>
        _bench = _BenchAsync[A](_name, this, _f, ops', _timeout)
        run()
      else _notify._result(name, ops, nspo)
      end
  
    be _failure(name: String, timeout: Bool) =>
      _notify._failure(name, timeout)

class _AutoOps
  let _bench_time: U64
  let _max_ops: U64

  new create(bench_time: U64, max_ops: U64) =>
    (_bench_time, _max_ops) = (bench_time, max_ops)

  fun apply(ops: U64, time: U64, nspo: U64): (U64 | None) =>
    if (time < _bench_time) and (ops < _max_ops) then
      var ops' = if nspo == 0 then
        _max_ops
      else
        _bench_time / nspo
      end
      ops' = (ops' + (ops' / 5)).min(ops * 100).max(ops + 1)
      _round_up(ops')
    else
      None
    end

  fun _round_down_10(x: U64): U64 =>
    """
    Round down to the nearest power of 10.
    """
    var tens: U64 = 0
    // tens = floor(log_10(n))
    var x' = x
    while x' >= 10 do
      x' = x' / 10
      tens = tens + 1
    end
    // result = 10^tens
    var result: U64 = 1
    for i in Range[U64](0, tens) do
      result = result * 10
    end
    result

  fun _round_up(x: U64): U64 =>
    """
    Round x up to a number of the form [1ex, 2ex, 3ex, 5ex].
    """
    let base = _round_down_10(x)
    if x <= base then
      base
    elseif x <= (base * 2) then
      base * 2
    elseif x <= (base * 3) then
      base * 3
    elseif x <= (base * 5) then
      base * 5
    else
      base * 10
    end
