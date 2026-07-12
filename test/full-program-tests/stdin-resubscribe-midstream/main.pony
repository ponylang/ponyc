"""
Disposing of stdin part way through its input, and subscribing again with a
smaller chunk.

The tester writes stdin.txt and closes. The program subscribes with a chunk of
512, and on its first bytes disposes of the input and subscribes again with a
chunk of 64. Every byte of stdin.txt has to arrive, in order, across the two
subscriptions, and the read after the last of them has to return zero.

Disposing with input still in the pipe and subscribing again is what this
reaches: the rest of stdin.txt is waiting when the first subscription goes away,
and the second, with a smaller chunk, has to read it from where the first left
off. No other test disposes with input still to come.

Each byte carries its own offset -- byte i is 33 + (i % 94), printable and never
a newline, so nothing rewrites it in a checkout -- and the program checks every
one of them, so a byte delivered out of order or twice fails here.
"""
use "files"

use @pony_exitcode[None](code: I32)

actor Main
  let _env: Env
  let _expected: USize
  var _offset: USize = 0
  var _resubscribed: Bool = false
  var _misplaced: Bool = false

  new create(env: Env) =>
    _env = env
    _expected = _ExpectedBytes(env)
    @pony_exitcode(1)
    env.input(recover iso _Notify(this, true) end, 512)

  be got(data: Array[U8] val) =>
    for byte in data.values() do
      if byte != _Pattern(_offset) then
        _misplaced = true
      end

      _offset = _offset + 1
    end

    if not _resubscribed then
      _resubscribed = true
      _env.input.dispose()
      _env.input(recover iso _Notify(this, false) end, 64)
    end

  be notifier_disposed(first: Bool) =>
    if first then
      // The program's own dispose, once it has read a chunk -- unless no chunk
      // ever arrived, in which case this is the end of stdin and there was
      // nothing to subscribe again over.
      if not _resubscribed then
        _env.err.print("stdin ended before any of it was read")
      end
    elseif _misplaced then
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
  let _first: Bool

  new iso create(main: Main, first: Bool) =>
    _main = main
    _first = first

  fun ref apply(data: Array[U8] iso) =>
    _main.got(consume data)

  fun ref dispose() =>
    _main.notifier_disposed(_first)
