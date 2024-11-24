use @pony_exitcode[None](code: I32)

use "actor_pinning"
use "time"

actor Main
  let _env: Env
  let _auth: PinUnpinActorAuth

  new create(env: Env) =>
    _env = env
    _auth = PinUnpinActorAuth(env.root)
    ActorPinning.pin(_auth)
    let interval: U64 = (10 * 1_000_000_000) / 10
    let timers = Timers
    let timer = Timer(Tick(this, 1), interval, interval)
    timers(consume timer)

  be check_pinned() =>
    if ActorPinning.is_successfully_pinned(_auth) then
      @pony_exitcode(5)
    else
      check_pinned()
    end

class Tick is TimerNotify
  let _main: Main
  var _tick_count: I64

  new iso create(main: Main, tick_count: I64) =>
    _main = main
    _tick_count = tick_count

    fun ref apply(timer: Timer, count: U64): Bool =>
      _tick_count = _tick_count - (count.i64())
      let done = _tick_count <= 0
      _main.check_pinned()
      not (done)
