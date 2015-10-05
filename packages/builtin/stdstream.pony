interface ByteSeq val
  """
  Accept both a String and an Array[U8].
  """
  fun size(): U64
  fun cstring(): Pointer[U8] tag
  fun apply(i: U64): U8 ?

interface ByteSeqIter val
  """
  Accept an iterable collection of String or Array[U8].
  """
  fun values(): Iterator[ByteSeq]

interface OutStream tag
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
    _stream = @os_stdout[Pointer[U8]]()

  new _err() =>
    """
    Create an async stream for stderr.
    """
    _stream = @os_stderr[Pointer[U8]]()

  be print(data: ByteSeq) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _write(data)
    _write("\n")

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
      _write(bytes)
      _write("\n")
    end

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      _write(bytes)
    end

  fun ref _write(data: ByteSeq) =>
    """
    Write the bytes without explicitly flushing.
    """
    @os_std_write[None](_stream, data.cstring(), data.size())
