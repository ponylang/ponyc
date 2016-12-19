use "promises"
use "time"

actor _BenchAsync[A: Any #share] is _Benchmark
  let _name: String
  let _notify: _BenchNotify
  let _f: {(): Promise[A] ?} val
  let _ops: U64
  let _timeout: U64
  var _start: U64 = 0
  var _cancel: Bool = false

  new create(
    name: String,
    notify: _BenchNotify,
    f: {(): Promise[A] ?} val,
    ops: U64,
    timeout: U64 = 0
  ) =>
    (_name, _notify, _f, _ops, _timeout) = (name, notify, f, ops, timeout)

  be run() =>
    @pony_triggergc[None](this)
    if _timeout > 0 then
      Timers.apply(Timer(
        object iso is TimerNotify
          let b: _BenchAsync[A] tag = this

          fun apply(timer: Timer ref, count: U64): Bool =>
            b.cancel()
            false

        end,
        _timeout
      ))
    end
    _start = Time.nanos()
    _run(0)

  be _run(i: U64) =>
    if _cancel then
      None
    elseif i == _ops then
      let t = Time.nanos() - _start
      _notify._result(_name, _ops, t / _ops)
    else
      try
        let n: _BenchAsync[A] tag = this
        _f().next[None]({(a: A)(n, i) => n._run(i+1)} iso)
      else
        _notify._failure(_name, false)
      end
    end

  be cancel() =>
    _cancel = true
    _notify._failure(_name, true)
