actor FileStream
  """
  Asynchronous access to a File object. Wraps file operations print, write,
  printv and writev.
  """
  let _file: File

  new create(file: File) =>
    _file = file

  be print(data: Bytes) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _file.print(data)

  be write(data: Bytes) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _file.write(data)

  be printv(data: BytesList) =>
    """
    Print an array of Bytes.
    """
    _file.printv(data)

  be writev(data: BytesList) =>
    """
    Write an array of Bytes.
    """
    _file.writev(data)

  fun ref _final() =>
    _file.dispose()