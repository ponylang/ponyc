"""
Regression test for https://github.com/ponylang/ponyc/issues/5741, a Windows
hang.

The tester holds this program's stdin open with nothing written to it for the
seconds in stdin-delay-seconds.txt, and closes it only then. The program
subscribes to stdin, and on its first timer tick disposes of the input. The
notifier has to be disposed of at once, or the exit code stays 1.

Disposing of stdin has to reach the notifier while the program is still waiting
for input. On Windows a read of stdin from a pipe used to run in the Stdin actor
on a scheduler thread and not return until the other end wrote a byte or closed,
so the dispose sent to Stdin waited in its queue behind the read.

The budget is half the delay, which the program reads from
stdin-delay-seconds.txt rather than repeating.

Stdin also disposes of a notifier when stdin ends, so the program fails a
disposal that arrives before it asked for one: that is a stdin the tester closed
early, and it would pass this test for the wrong reason.
"""
use "files"
use "time"

use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  let _timers: Timers = Timers
  let _start: U64 = Time.millis()
  let _budget_millis: U64
  var _asked: Bool = false

  new create(env: Env) =>
    _env = env
    _budget_millis = _DelayMillis(env) / 2
    @pony_exitcode(1)
    env.input(recover iso _Notify(this) end, 64)
    _timers(Timer(_TickNotify(this), 100_000_000, 100_000_000))

  be tick() =>
    _asked = true
    _env.input.dispose()

  be disposed() =>
    let elapsed = Time.millis() - _start

    if not _asked then
      // Stdin disposes of a notifier at the end of stdin as well as when it is
      // asked to, and only the second is what this test measures.
      _env.err.print("stdin ended before the program disposed of it")
    elseif (_budget_millis > 0) and (elapsed < _budget_millis) then
      @pony_exitcode(0)
    else
      _env.err.print("stdin was disposed of after " + elapsed.string()
        + "ms; the budget is " + _budget_millis.string() + "ms")
    end

    _timers.dispose()

primitive _DelayMillis
  fun apply(env: Env): U64 =>
    """
    How long the tester holds stdin open, from the stdin-delay-seconds.txt in
    the program's working directory. Zero when it cannot be read, which fails
    the test rather than passing it by accident.
    """
    try
      let path = FilePath(FileAuth(env.root), "stdin-delay-seconds.txt")
      let file = OpenFile(path) as File
      (file.read_string(file.size()).clone() .> strip()).u64()? * 1000
    else
      env.err.print("unable to read stdin-delay-seconds.txt")
      0
    end

class _Notify is InputNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref dispose() =>
    _main.disposed()

class _TickNotify is TimerNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(timer: Timer, count: U64): Bool =>
    _main.tick()
    true
