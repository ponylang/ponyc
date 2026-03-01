"""
# Debug package

Provides facilities to create output to either `STDOUT` or `STDERR` that will
only appear when the platform is debug configured. To create a binary with
debug configured, pass the `-d` flag to `ponyc` when compiling e.g.:

`ponyc -d`

## Example code

```pony
use "debug"

actor Main
  new create(env: Env) =>
    Debug.out("This will only be seen when configured for debug info")
    env.out.print("This will always be seen")
```
"""
use @fprintf[I32](stream: Pointer[U8] tag, fmt: Pointer[U8] tag, ...)
use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()

primitive DebugOut
primitive DebugErr

type DebugStream is (DebugOut | DebugErr)

primitive Debug
  """
  This is a debug only print utility.
  """
  fun apply(
    msg: (Stringable | ReadSeq[Stringable]),
    sep: String = ", ",
    stream: DebugStream = DebugOut)
  =>
    """
    If platform is debug configured, print either a single stringable or a
    sequence of stringables. The default separator is ", ", and the default
    output stream is stdout.
    """
    ifdef debug then
      match \exhaustive\ msg
      | let m: Stringable =>
        _print(m.string(), stream)
      | let m: ReadSeq[Stringable] =>
        _print(sep.join(m.values()), stream)
      end
    end

  fun out(msg: Stringable = "") =>
    """
    If platform is debug configured, print message to standard output
    """
    _print(msg.string(), DebugOut)

  fun err(msg: Stringable = "") =>
    """
    If platform is debug configured, print message to standard error
    """
    _print(msg.string(), DebugErr)

  fun _print(msg: String, stream: DebugStream) =>
    ifdef debug then
      @fprintf(_stream(stream), "%s\n".cstring(), msg.cstring())
    end

  fun _stream(stream: DebugStream): Pointer[U8] =>
    match \exhaustive\ stream
    | DebugOut => @pony_os_stdout()
    | DebugErr => @pony_os_stderr()
    end
