"""
Reading stdin from a pipe that carries no input for a while, and then does.

The tester holds this program's stdin open with nothing written to it for the
seconds in stdin-delay-seconds.txt, then writes stdin.txt and closes. The
program runs with one scheduler thread, and exits 0 only if a timer fired before
the first byte arrived, every byte of stdin.txt arrived, and the read after them
returned zero.

The tick has to come first, so the program has to be running while it waits for
input. On Windows a read of stdin from a pipe used to hold the one scheduler
thread until the other end wrote a byte, so the first thing that ran was the
read.

stdin.txt is many chunks long, so it arrives across many reads. How many bytes
to expect is the size of stdin.txt, which the program reads rather than
repeating.
"""
use "files"
use "time"

use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  let _timers: Timers = Timers
  let _expected: USize
  var _ticked: Bool = false
  var _ticked_before_data: Bool = false
  var _bytes: USize = 0

  new create(env: Env) =>
    _env = env
    _expected = _ExpectedBytes(env)
    @pony_exitcode(1)
    env.input(recover iso _Notify(this) end, 64)
    _timers(Timer(_TickNotify(this), 100_000_000, 100_000_000))

  be tick() =>
    _ticked = true

  be got(count: USize) =>
    if _bytes == 0 then
      // A tick after the first byte proves nothing, so the check is made here.
      _ticked_before_data = _ticked
    end

    _bytes = _bytes + count

  be ended() =>
    if not _ticked_before_data then
      _env.err.print("no timer fired before the first byte of stdin arrived")
    end

    if _bytes != _expected then
      _env.err.print("read " + _bytes.string() + " bytes, expected "
        + _expected.string())
    end

    if _ticked_before_data and (_expected > 0) and (_bytes == _expected) then
      @pony_exitcode(0)
    end

    _timers.dispose()

primitive _ExpectedBytes
  fun apply(env: Env): USize =>
    """
    How many bytes the tester writes, from the size of the stdin.txt in the
    program's working directory. Zero when it cannot be read, which fails the
    test rather than passing it by accident.
    """
    try
      FileInfo(FilePath(FileAuth(env.root), "stdin.txt"))?.size
    else
      env.err.print("unable to size stdin.txt")
      0
    end

class _Notify is InputNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(data: Array[U8] iso) =>
    _main.got(data.size())

  fun ref dispose() =>
    _main.ended()

class _TickNotify is TimerNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(timer: Timer, count: U64): Bool =>
    _main.tick()
    true
