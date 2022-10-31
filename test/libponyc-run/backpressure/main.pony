use @printf[I32](fmt: Pointer[U8] tag, ...)
use @pony_get_exitcode[I32]()
use @pony_exitcode[None](code: I32)
use "backpressure"
use "time"

actor Incrementer
  let _underpressure: UnderPressure
  var _i: I32 = 0
  var _stop: Bool = false

  new create(env: Env) =>
    _underpressure = UnderPressure(env, this)

  be keep_counting() =>
    _i = _i + 1
    @pony_exitcode(_i)
    if (_i % 10) == 0 then
      _underpressure.do_stuff()
    end
    if not _stop then
      keep_counting()
    end

  be stop_counting() =>
    _stop = true

actor UnderPressure
  let _timers: Timers = Timers
  let _timer: Timer tag
  let _env: Env
  let _inc: Incrementer
  var _i: I32 = 0

  new create(env: Env, inc: Incrementer) =>
    _env = env
    _inc = inc

    Backpressure.apply(ApplyReleaseBackpressureAuth(_env.root))
    _inc.keep_counting()

    let timer = Timer(UnderPressureWaker(this), 500_000_000, 500_000_000)
    _timer = timer
    _timers(consume timer)

  be release_backpressure() =>
    Backpressure.release(ApplyReleaseBackpressureAuth(_env.root))
    _inc.stop_counting()

  be do_stuff() =>
    _i = _i + 1


class UnderPressureWaker is TimerNotify
  let _underpressure: UnderPressure
  new iso create(underpressure: UnderPressure) =>
    _underpressure = underpressure

  fun ref apply(timer: Timer, count: U64): Bool =>
    _underpressure.release_backpressure()
    false

actor Main
  new create(env: Env) =>
    Incrementer(env)
