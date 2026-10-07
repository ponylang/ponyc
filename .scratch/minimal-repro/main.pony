use "collections"
use "time"

actor Main
  new create(env: Env) =>
    let timers = Timers
    let timer = Timer(OneShot(env), 1_000_000_000, 0)
    timers(consume timer)

    for cluster in Range(0, 10) do
      let a = RingActor
      let b = RingActor
      let c = RingActor
      a.set_next(b)
      b.set_next(c)
      c.set_next(a)
      a.pass(10)
    end

class OneShot is TimerNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(timer: Timer, count: U64): Bool =>
    _env.out.print("timer fired")
    false

actor RingActor
  var _next: (RingActor | None) = None

  be set_next(next: RingActor) =>
    _next = next

  be pass(n: U64) =>
    if n > 0 then
      match _next
      | let r: RingActor => r.pass(n - 1)
      end
    end
