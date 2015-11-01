actor FileStream is OutStream
  """
  Asynchronous access to a File object. Wraps file operations print, write,
  printv and writev. The File will be disposed through File._final.
  """
  let _file: File

  new create(file: File iso) =>
    _file = consume file

  be print(data: ByteSeq) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _file.print(data)

  be write(data: ByteSeq) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _file.write(data)

  be printv(data: ByteSeqIter) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    _file.printv(data)

  be writev(data: ByteSeqIter) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    _file.writev(data)
