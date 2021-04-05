use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()
use @pony_os_std_flush[None](file: Pointer[None] tag)
use @pony_os_std_print[None](file: Pointer[None] tag, buffer: Pointer[U8] tag,
  size: USize)
use @pony_os_std_write[None](file: Pointer[None] tag, buffer: Pointer[U8] tag,
  size: USize)
type ByteSeq is (String | Array[U8] val)

interface val ByteSeqIter
  """
  Accept an iterable collection of String or Array[U8] val.
  """
  fun values(): Iterator[this->ByteSeq box]

interface tag OutStream
  """
  Asnychronous access to some output stream.
  """
  be print(data: ByteSeq)
    """
    Print some bytes and insert a newline afterwards.
    """

  be write(data: ByteSeq)
    """
    Print some bytes without inserting a newline afterwards.
    """

  be printv(data: ByteSeqIter)
    """
    Print an iterable collection of ByteSeqs.
    """

  be writev(data: ByteSeqIter)
    """
    Write an iterable collection of ByteSeqs.
    """

  be flush()
    """
    Flush the stream.
    """

actor StdStream
  """
  Asynchronous access to stdout and stderr. The constructors are private to
  ensure that access is provided only via an environment.
  """
  var _stream: Pointer[U8]

  new _out() =>
    """
    Create an async stream for stdout.
    """
    _stream = @pony_os_stdout()

  new _err() =>
    """
    Create an async stream for stderr.
    """
    _stream = @pony_os_stderr()

  be print(data: ByteSeq) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _print(data)

  be write(data: ByteSeq) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _write(data)

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _print(bytes)
    end

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _write(bytes)
    end

  be flush() =>
    """
    Flush any data out to the os (ignoring failures).
    """
    @pony_os_std_flush(_stream)

  fun ref _write(data: ByteSeq) =>
    """
    Write the bytes without explicitly flushing.
    """
    @pony_os_std_write(_stream, data.cpointer(), data.size())

  fun ref _print(data: ByteSeq) =>
    """
    Write the bytes and a newline without explicitly flushing.
    """
    @pony_os_std_print(_stream, data.cpointer(), data.size())
