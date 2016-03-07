primitive DebugOut
primitive DebugErr

type DebugStream is (DebugOut | DebugErr)

primitive Debug
  """
  This is a debug only print utility.
  """
  fun apply(msg: Stringable, sep: String, stream: DebugStream = DebugOut) =>
    """
    If platform is debug configured, print a stringable. The default output
    stream is stdout.
    """
    ifdef debug then
      _print(msg.string(), stream)
    end

  fun apply(msg: ReadSeq[Stringable], sep: String = ", ",
    stream: DebugStream)
  =>
    """
    If platform is debug configured, print a sequence of stringables. The
    default separator is ", ", and the default output stream is stdout.
    """
    ifdef debug then
      _print(sep.join(msg), stream)
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
      try
        @fprintf[I32](_stream(stream), "%s\n".cstring(), msg.cstring())
      end
    end

  fun _stream(stream: DebugStream): Pointer[U8] ? =>
    match stream
    | DebugOut => @pony_os_stdout[Pointer[U8]]()
    | DebugErr => @pony_os_stderr[Pointer[U8]]()
    else
      error
    end
