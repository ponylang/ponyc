use "time"

class TimerPrint is TimerNotify
  var _count: U64 = 0

  new iso create() =>
    None

  fun ref apply(timer: Timer, count: U64): Bool =>
    _count = _count + count
    _count < 10

  fun ref cancel(timer: Timer) =>
    None

actor Main
  new create(env: Env) =>
    let timers = Timers

    let t1 = Timer(TimerPrint, 500000000, 500000000) // 500 ms
    let t2 = Timer(TimerPrint, 500000000, 500000000) // 500 ms

    let t1' = t1
    timers(consume t1)
    timers.cancel(t1')
    timers(consume t2)
