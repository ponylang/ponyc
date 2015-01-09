use "time"
use "events"

class TimerPrint is TimerNotify
  var _env: Env
  var _count: U64 = 0

  new create(env: Env) =>
    _env = env

  fun ref apply(timer: Timer, count: U64): Bool =>
    _count = _count + count
    _env.out.print("timer: " + _count.string())
    _count < 10

  fun ref cancel(timer: Timer) =>
    _env.out.print("timer cancelled")

actor Main
  new create(env: Env) =>
    let timers = Timers(0)

    let t1 = recover val
      Envelope[Timer](timers,
        recover
          Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
        end
        )
    end

    timers(t1)
    timers.cancel(t1)

    let t2 = recover val
      Envelope[Timer](timers,
        recover
          Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
        end
        )
    end

    timers(t2)
