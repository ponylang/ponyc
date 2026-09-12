use "time"

actor Main
  new create(env: Env) =>
    let timers = Timers

    let t1 = Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
    let t1' = t1
    timers(consume t1)
    timers.cancel(t1')

    let t2 = Timer(TimerPrint(env), 500000000, 500000000) // 500 ms
    timers(consume t2)
