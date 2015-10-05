use "time"

class TimerPrint is TimerNotify
  var _env: Env
  var _count: U64 = 0

  new iso create(env: Env) =>
    _env = env

  fun ref apply(timer: Timer, count: U64): Bool =>
    _count = _count + count
    _env.out.print("timer: " + _count.string())
    _count < 10

  fun ref cancel(timer: Timer) =>
    _env.out.print("timer cancelled")

actor Main
  new create(env: Env) =>
    let timers = Timers

    let t1 = Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
    let t1' = t1
    timers(consume t1)
    timers.cancel(t1')

    let t2 = Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
    timers(consume t2)
