interface Bytes val
  """
  Accept both a String and an Array[U8].
  """
  fun size(): U64
  fun cstring(): Pointer[U8] tag

interface BytesList val
  """
  Accept an iterable collection of String or Array[U8].
  """
  fun values(): Iterator[Bytes]

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

  be print(data: Bytes) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _write(data)
    _write("\n")

  be write(data: Bytes) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _write(data)

  be printv(data: BytesList) =>
    """
    Print an array of Bytes.
    """
    try
      for bytes in data.values() do
        _write(bytes)
        _write("\n")
      end
    end

  be writev(data: BytesList) =>
    """
    Write an array of Bytes.
    """
    try
      for bytes in data.values() do
        _write(bytes)
      end
    end

  fun ref _write(data: Bytes) =>
    """
    Write the bytes without explicitly flushing.
    """
    if Platform.linux() then
      @fwrite_unlocked[U64](data.cstring(), U64(1), data.size(), _stream)
    else
      @fwrite[U64](data.cstring(), U64(1), data.size(), _stream)
    end
