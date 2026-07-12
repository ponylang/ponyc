"""
Reading a stdin larger than the pipe buffer.

The tester writes stdin.txt and closes. The program reads it in 64-byte chunks,
and exits 0 only if every byte arrives, in order, and the read after the last of
them returns zero.

stdin.txt is larger than the pipe buffer the process package gives a child's
stdin, so writing it is more than one write. A write to a Windows PIPE_NOWAIT
pipe writes all of the request or none of it, so a write bigger than the buffer
writes nothing to a reader that is not blocked in a read -- and this program is
never blocked, it peeks. The process package caps each write to the buffer, so
the write becomes writes that fit. Without that, this input never arrives.

Each byte carries its own offset -- byte i is 33 + (i % 94), printable and never
a newline, so nothing rewrites it in a checkout -- and the program checks every
one of them, so a byte delivered out of order, twice, or short fails here.
"""
use "files"

use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  let _expected: USize
  var _offset: USize = 0
  var _misplaced: Bool = false

  new create(env: Env) =>
    _env = env
    _expected = _ExpectedBytes(env)
    @pony_exitcode(1)
    env.input(recover iso _Notify(this) end, 64)

  be got(data: Array[U8] val) =>
    for byte in data.values() do
      if byte != _Pattern(_offset) then
        _misplaced = true
      end

      _offset = _offset + 1
    end

  be ended() =>
    if _misplaced then
      _env.err.print("a byte of stdin arrived out of place")
    elseif _offset != _expected then
      _env.err.print("read " + _offset.string() + " bytes, expected "
        + _expected.string())
    elseif _expected > 0 then
      @pony_exitcode(0)
    end

primitive _Pattern
  fun apply(offset: USize): U8 =>
    """
    The byte stdin.txt carries at this offset.
    """
    33 + (offset % 94).u8()

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
    _main.got(consume data)

  fun ref dispose() =>
    _main.ended()
