primitive DebugOut
primitive DebugErr

type DebugStream is (DebugOut | DebugErr)

primitive Debug
  """
  This is a debug only print utility.
  """
  fun apply(data: (Stringable | ReadSeq[Stringable]),
    sep: String = ", ", stream: DebugStream = DebugOut)
  =>
    """
    If platform is debug configured, print either a single stringable or a
    sequence of stringables. The default separator is ", ", and the default
    output stream is stdout.
    """
    if Platform.debug() then
      match data
      | let arg: Stringable =>
        _print(stream, arg.string() + "\n")
      | let args: ReadSeq[Stringable] =>
        let buf = recover String end

        if args.size() > 0 then
          for arg in args.values() do
            buf.append(arg.string())
            buf.append(sep)
          end

          buf.truncate(buf.size() - sep.size())
          buf.append("\n")

          _print(stream, consume buf)
        end
      end
    end

  fun out(msg: Stringable = "") =>
    """
    If platform is debug configured, print message to standard output
    """
    _print(DebugOut, msg.string() + "\n")

  fun err(msg: Stringable = "") =>
    """
    If platform is debug configured, print message to standard error
    """
    _print(DebugErr, msg.string() + "\n")

  fun _print(stream: DebugStream, msg: String) =>
    if Platform.debug() then
      if msg.size() > 0 then
        try
          @fwrite[U64](msg.cstring(), U64(1), msg.size(), _stream(stream))
        end
      end
    end

  fun _stream(stream: DebugStream): Pointer[U8] ? =>
    match stream
    | DebugOut => @os_stdout[Pointer[U8]]()
    | DebugErr => @os_stderr[Pointer[U8]]()
    else
      error
    end
