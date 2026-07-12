"""
Regression test for https://github.com/ponylang/ponyc/issues/5741, a Windows
hang.

The tester holds this program's stdin open with nothing written to it for the
seconds in stdin-delay-seconds.txt, and closes it only then. The program runs
with one scheduler thread, subscribes to stdin, and starts a timer. The timer
has to fire while the program is still waiting for input, or the exit code stays
1.

On Windows a read of stdin from a pipe used to run in the Stdin actor on a
scheduler thread and not return until the other end wrote a byte or closed. With
one scheduler thread that was the whole program: no timer fired and no other
actor ran until the tester closed stdin.

The budget is half the delay, which the program reads from
stdin-delay-seconds.txt rather than repeating. The program-args.txt is what
gives the program its one scheduler thread, and the program checks that it got
it: with more than one, a tick would prove nothing about the thread the read is
on. It also fails a tick that arrives after stdin has ended, which is a stdin
the tester closed early.
"""
use "files"
use "runtime_info"
use "time"

use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  let _timers: Timers = Timers
  let _start: U64 = Time.millis()
  let _budget_millis: U64
  let _schedulers: U32
  var _stdin_ended: Bool = false

  new create(env: Env) =>
    _env = env
    _budget_millis = _DelayMillis(env) / 2
    _schedulers = Scheduler.schedulers(SchedulerInfoAuth(env.root))
    @pony_exitcode(1)
    env.input(recover iso _Notify(this) end, 64)
    _timers(Timer(_TickNotify(this), 100_000_000, 100_000_000))

  be stdin_ended() =>
    _stdin_ended = true

  be tick() =>
    let elapsed = Time.millis() - _start

    if _schedulers != 1 then
      _env.err.print("the program ran with " + _schedulers.string()
        + " scheduler threads, not 1")
    elseif _stdin_ended then
      _env.err.print("stdin ended before the first timer tick")
    elseif (_budget_millis > 0) and (elapsed < _budget_millis) then
      @pony_exitcode(0)
    else
      _env.err.print("the first timer tick came after " + elapsed.string()
        + "ms; the budget is " + _budget_millis.string() + "ms")
    end

    _env.input.dispose()
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
    _main.stdin_ended()

class _TickNotify is TimerNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(timer: Timer, count: U64): Bool =>
    _main.tick()
    false
