use "time"

class TimerPrint is TimerNotify
  """
  Prints a running count on each timer tick and stops after 10.
  """
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
