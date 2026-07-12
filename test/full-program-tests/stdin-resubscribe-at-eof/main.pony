"""
Regression test for https://github.com/ponylang/ponyc/issues/5737, a Windows
hang.

This test's stdin.txt is empty, so the program's stdin is closed with nothing in
it and every read is at the end of stdin. Each read returns zero bytes, the
notifier is disposed of, and the program subscribes again. Four rounds, then it
exits.

Four rounds and not two: a readiness packet left over from the previous
subscription's wait on stdin can supply the first resubscription with its read.
The subscription after that one is the one that gets no read and stays live, so
the test has to reach it.

The exit code is 1 until all four rounds have reached the end of stdin, so a run
that exits without reading fails.
"""
use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  var _eofs: USize = 0

  new create(env: Env) =>
    _env = env
    @pony_exitcode(1)
    _subscribe()

  be _subscribe() =>
    _env.input(recover iso _Notify(this) end, 64)

  be saw_eof() =>
    _eofs = _eofs + 1

    if _eofs < 4 then
      _subscribe()
    else
      @pony_exitcode(0)
    end

class _Notify is InputNotify
  let _main: Main

  new iso create(main: Main) =>
    _main = main

  fun ref apply(data: Array[U8] iso) =>
    None

  fun ref dispose() =>
    _main.saw_eof()
