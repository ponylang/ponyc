"""
Reading stdin to its end, in chunks. The tester writes stdin.txt to this
program's stdin and closes it, so the reads return 5000 bytes in total and then
zero bytes.

Every chunk read has to be followed by another ASIO_READ, or the program stops
part way through its input and never exits. The exit code is 1 until all 5000
bytes have arrived and a read has returned zero, so a run that stops early
fails.
"""
use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  var _bytes: USize = 0

  new create(env: Env) =>
    _env = env
    @pony_exitcode(1)
    _env.input(recover iso _Notify(this) end, 64)

  be got(count: USize) =>
    _bytes = _bytes + count

  be ended() =>
    if _bytes == 5000 then
      @pony_exitcode(0)
    else
      _env.err.print("read " + _bytes.string() + " bytes, expected 5000")
    end

class _Notify is InputNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(data: Array[U8] iso) =>
    _main.got(data.size())

  fun ref dispose() =>
    _main.ended()
