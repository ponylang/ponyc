use "collections"
use "promises"
use "time"

actor _BenchAsync[A: Any #share]
  let _notify: _BenchNotify

  new create(notify: _BenchNotify) =>
    _notify = notify

  be apply(name: String, f: {(): Promise[A] ?} val, ops: U64) =>
    try
      @pony_triggergc[None](this)
      let pn = _PromiseNotify(_notify, name, ops)
      for i in Range[U64](0, ops) do
        let p = f()
        p.next[A]({(a: A)(pn): A => pn(); a } iso)
      end
    else
      _notify._failure(name)
    end

actor _PromiseNotify
  let _notify: _BenchNotify
  let _name: String
  let _ops: U64
  var _rem: U64
  var _start: U64

  new create(notify: _BenchNotify, name: String, ops: U64) =>
    (_notify, _name, _ops) = (notify, name, ops)
    _rem = ops
    _start = Time.nanos()

  be apply() =>
    if _rem == 1 then
      let t = Time.nanos() - _start
      _notify._result(_name, _ops, t/_ops)
    else
      _rem = _rem - 1
    end
