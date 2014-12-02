interface Bytes
  """
  Accept both a String and an Array[U8].
  """
  fun box size(): U64
  fun box cstring(): Pointer[U8] tag

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

  be print(data: Bytes val) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _write(data)
    _write("\n")

  be write(data: Bytes val) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _write(data)

  fun ref _write(data: Bytes val) =>
    """
    Write the bytes without explicitly flushing.
    """
    if Platform.linux() then
      @fwrite_unlocked[U64](data.cstring(), U64(1), data.size(), _stream)
    else
      @fwrite[U64](data.cstring(), U64(1), data.size(), _stream)
    end
