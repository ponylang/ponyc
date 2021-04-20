actor FileStream[E: StringEncoder val = UTF8StringEncoder] is OutStream
  """
  Asynchronous access to a File object. Wraps file operations print, write,
  printv and writev. The File will be disposed through File._final.
  """
  let _file: File

  new create(file: File iso) =>
    _file = consume file

  be print(data: (String | ByteSeq)) =>
    """
    Print some bytes and insert a newline afterwards.
    """
    _file.write[E](data)
    _file.write[E]("\n")

  be write(data: (String | ByteSeq)) =>
    """
    Print some bytes without inserting a newline afterwards.
    """
    _file.write[E](data)

  be printv(data: (StringIter | ByteSeqIter)) =>
    """
    Print an iterable collection of ByteSeqs.
    """
    _file.printv[E](data)

  be writev(data: (StringIter | ByteSeqIter)) =>
    """
    Write an iterable collection of ByteSeqs.
    """
    _file.writev[E](data)

  be flush() =>
    """
    Flush pending data to write.
    """
    _file.flush()
