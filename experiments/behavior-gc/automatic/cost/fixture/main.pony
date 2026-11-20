use "time"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @pony_exitcode[None](code: I32)

class \nodoc\ Cell
  let value: U64

  new create(n: U64) => value = n

actor \nodoc\ Probe
  let _limit: U64
  var _round: U64 = 0
  var _start: U64 = 0
  var _sum: U64 = 0
  var _mix: U64 = 1
  var _previous: Cell = Cell(0)
  var _inner: Cell = Cell(0)

  new create(n: U64) =>
    _limit = n
    start()

  be start() =>
    _start = Time.nanos()
    measure()

  be measure() =>
    let outer = Cell(_round)
    middle()
    _sum = _sum + _inner.value + _previous.value + _mix
    _previous = outer
    _round = _round + 1
    if _round < _limit then
      measure()
    else
      let elapsed = Time.nanos() - _start
      @printf("%llu %llu\n".cstring(), _sum, elapsed)
      @pony_exitcode(0)
    end

  fun \noinline\ ref middle() =>
    let inner = Cell(_round + 3)
    phase()
    _inner = inner

  fun \noinline\ ref phase() =>
    mix()
    finish()

  fun \noinline\ ref mix() => _mix = (_mix * 17) + _round

  fun \noinline\ ref finish() => _mix = _mix + 1

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    try Probe(env.args(1)?.u64()?) end
