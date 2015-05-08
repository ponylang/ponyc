interface Bytes val
  """
  Accept both a String and an Array[U8].
  """
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

  be print_color(color: U8, data: Bytes) =>
    """
    Print some bytes in the specified color and insert a newline afterwards.
    The color numbering used is the ANSI escape codes, suitable constant
    methods are defined in env.
    """
    @os_set_color[None](_stream, color)
    _write(data)
    @os_reset_color[None]()
    _write("\n")

  be write(data: Bytes) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _write(data)

  be write_color(color: U8, data: Bytes) =>
    """
    Print some bytes in the specified color without inserting a newline
    afterwards.
    The color numbering used is the ANSI escape codes, suitable constant
    methods are defined in env.
    """
    @os_set_color[None](_stream, color)
    _write(data)
    @os_reset_color[None]()

  be printv(data: BytesList) =>
    """
    Print an array of Bytes.
    """
    for bytes in data.values() do
      _write(bytes)
      _write("\n")
    end

  be writev(data: BytesList) =>
    """
    Write an array of Bytes.
    """
    for bytes in data.values() do
      _write(bytes)
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
