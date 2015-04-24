interface Bytes val
  """
  Accept both a String and an Array[U8].
  """
  new create(len: U64)

  fun size(): U64
  fun cstring(): Pointer[U8] tag

  fun apply(i: U64): U8 ?
  fun ref update(i: U64, value: U8): U8 ?

interface BytesList val
  """
  Accept an iterable collection of String or Array[U8].
  """
  fun values(): Iterator[Bytes]

interface Stream tag
  """
  Asnychronous access to some output stream.
  """
  be print(data: Bytes)
    """
    Print some bytes and insert a newline afterwards.
    """

  be write(data: Bytes)
    """
    Print some bytes without inserting a newline afterwards.
    """

  be printv(data: BytesList)
    """
    Print an array of Bytes.
    """

  be writev(data: BytesList)
    """
    Write an array of Bytes.
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
    let len = data.size()
    var offset = U64(0)

    while offset < len do
      let r =
        if Platform.linux() then
          @fwrite_unlocked[U64](data.cstring()._offset_tag(offset),
            U64(1), data.size() - offset, _stream)
        else
          @fwrite[U64](data.cstring()._offset_tag(offset),
            U64(1), data.size() - offset, _stream)
        end

      offset = offset + r
    end
