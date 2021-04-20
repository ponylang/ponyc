type ByteSeq is (Array[U8] val)

interface val ByteSeqIter
  """
  An iterable collection of Array[U8] val.
  """
  fun values(): Iterator[this->ByteSeq box]

interface val StringIter
  """
  An iterable collection of String val.
  """
  fun values(): Iterator[this->String box]

interface tag OutStream
  """
  Asnychronous access to some output stream.
  """

  be print(data: (String | ByteSeq))
    """
    Print a String or some bytes and insert a newline afterwards.
    """

  be write(data: (String | ByteSeq))
    """
    Print a String or some bytes without inserting a newline afterwards.
    """

  be printv(data: (StringIter | ByteSeqIter))
    """
    Print an iterable collection of Strings or ByteSeqs using the default encoding (UTF-8).
    """

  be writev(data: (StringIter | ByteSeqIter))
    """
    Write an iterable collection of Strings or ByteSeqs using the default encoding (UTF-8).
    """

  be flush()
    """
    Flush the stream.
    """

actor StdStream is OutStream
  """
  Asynchronous access to stdout and stderr. The constructors are private to
  ensure that access is provided only via an environment.
  """
  var _stream: Pointer[None]

  new _out() =>
    """
    Create an async stream for stdout.
    """
    _stream = @pony_os_stdout[Pointer[None]]()

  new _err() =>
    """
    Create an async stream for stderr.
    """
    _stream = @pony_os_stderr[Pointer[None]]()

  be print(data: (String | ByteSeq)) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    match data
    | let s: (String) =>
      _print(s.array())
    | let d: (ByteSeq) =>
      _print(d)
    end

  be write(data: (String | ByteSeq)) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    match data
    | let s: (String) =>
      _write(s.array()) // Ignore the specified encoder
    | let d: (ByteSeq) =>
      _write(d)
    end

  be printv(data: (StringIter | ByteSeqIter)) =>
    """
    Print an iterable collection of Strings or ByteSeqs.
    """
    match data
    | let si: (StringIter val) =>
      for string in si.values() do
        _print(string.array())
      end
    | let bsi: (ByteSeqIter val) =>
      for bytes in bsi.values() do
        _print(bytes)
      end
    end

  be writev(data: (StringIter | ByteSeqIter)) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    match data
    | let si: (StringIter val) =>
      for string in si.values() do
        _write(string.array())
      end
    | let bsi: (ByteSeqIter val) =>
      for bytes in bsi.values() do
        _write(bytes)
      end
    end

  be flush() =>
    """
    Flush any data out to the os (ignoring failures).
    """
    @pony_os_std_flush[None](_stream)

  fun ref _write(data: ByteSeq) =>
    """
    Write the bytes without explicitly flushing.
    """
    @pony_os_std_write[None](_stream, data.cpointer(), data.size())

  fun ref _print(data: ByteSeq) =>
    """
    Write the bytes and a newline without explicitly flushing.
    """
    @pony_os_std_print[None](_stream, data.cpointer(), data.size())
