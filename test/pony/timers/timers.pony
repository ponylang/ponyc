use "time"

class TimerPrint is TimerNotify
  var _env: Env
  var _count: U64 = 0

  new create(env: Env) =>
    _env = env

  fun ref apply(timer: Timer, count: U64): Bool =>
    _count = _count + count
    _env.out.print("timer: " + _count.string())
    _count < 10

actor Main
  new create(env: Env) =>
    let timers = Timers(0)
    let timer = recover
      Timer.create(TimerPrint(env), 500000000, 500000000) // 500 ms
    end
    timers.set(consume timer)
