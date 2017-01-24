use "collections"
use "time"

actor _Bench[A: Any #share] is _Benchmark
  let _name: String
  let _notify: _BenchNotify
  let _f: {(): A ?} val
  let _ops: U64

  new create(name: String, notify: _BenchNotify, f: {(): A ?} val, ops: U64) =>
    (_name, _notify, _f, _ops) = (name, notify, f, ops)

  be run() =>
    try
      @pony_triggergc[None](this)
      let start = Time.nanos()
      for i in Range[U64](0, _ops) do
        DoNotOptimise[A](_f())
      end
      let t = Time.nanos() - start
      _notify._result(_name, _ops, t/_ops)
    else
      _notify._failure(_name, false)
    end
