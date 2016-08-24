use "collections"
use "time"

actor _Bench[A: Any #share]
  let _notify: _BenchNotify

  new create(notify: _BenchNotify) =>
    _notify = notify

  be apply(name: String, f: {(): A ?} val, ops: U64) =>
    try
      @pony_triggergc[None](this)
      let start = Time.nanos()
      for i in Range[U64](0, ops) do
        DoNotOptimise[A](f())
      end
      let t = Time.nanos() - start
      _notify._result(name, ops, t/ops)
    else
      _notify._failure(name)
    end
