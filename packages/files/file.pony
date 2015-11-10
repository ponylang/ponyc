primitive _FileHandle

primitive FileOK
primitive FileError
// TODO: break error down into multiple types

type FileErrNo is
  ( FileOK
  | FileError
  )

primitive CreateFile
  """
  Open a File for read/write, creating if it doesn't exist, preserving the
  contents if it does exist.
  """
  fun apply(from: FilePath): (File | FileErrNo) =>
    let file = File(from)
    let err = file.errno()
    
    match err
    | FileOK => file
    else
      err
    end
    
primitive OpenFile
  """
  Open a File for read only.
  """
  fun apply(from: FilePath): (File | FileErrNo) =>
    let file = File.open(from)
    let err = file.errno()
    
    match err
    | FileOK => file
    else
      err
    end

class File
  """
  Operations on a file.
  """
  let path: FilePath
  let writeable: Bool
  var _fd: I32
  var _handle: Pointer[_FileHandle]
  var _last_line_length: U64 = 256
  var _errno: FileErrNo = FileOK

  new create(from: FilePath) =>
    """
    Attempt to open for read/write, creating if it doesn't exist, preserving
    the contents if it does exist.
    Set errno according to result.
    """
    path = from
    writeable = true
    _fd = -1
    _handle = Pointer[_FileHandle]

    if not from.caps(FileRead) or not from.caps(FileWrite) then
      _errno = FileError
    else
      var mode = "x"
      if not from.exists() then
        if not from.caps(FileCreate) then
          _errno = FileError
        else
          mode = "w+b"
        end
      else
        mode = "r+b"
      end

      if mode != "x" then
        _handle = @fopen[Pointer[_FileHandle]](from.path.cstring(),
          mode.cstring())

        if _handle.is_null() then
          _errno = FileError
        else
          _fd = _get_fd(_handle)

          try
            _FileDes.set_rights(_fd, path, writeable)
          else
            _errno = FileError
          end
        end
      end
    end

  new open(from: FilePath) =>
    """
    Open for read only.
    Set _errno according to result.
    """
    path = from
    writeable = false
    _fd = -1
    _handle = Pointer[_FileHandle]

    if not from.caps(FileRead) then
      _errno = FileError
    else
      _handle = @fopen[Pointer[_FileHandle]](from.path.cstring(), "rb".cstring())

      if _handle.is_null() then
        _errno = FileError
      else
        _fd = _get_fd(_handle)

        try
          _FileDes.set_rights(_fd, path, writeable)
        else
          _errno = FileError
        end
      end
    end

  new _descriptor(fd: I32, from: FilePath) ? =>
    """
    Internal constructor from a file descriptor and a path.
    """
    if not from.caps(FileRead) or (fd == -1) then
      error
    end

    path = from
    writeable = from.caps(FileRead)
    _fd = fd

    if writeable then
      _handle = @fdopen[Pointer[_FileHandle]](fd, "r+b".cstring())
    else
      _handle = @fdopen[Pointer[_FileHandle]](fd, "rb".cstring())
    end

    if _handle.is_null() then
      error
    end

    _FileDes.set_rights(_fd, path, writeable)

  fun errno(): FileErrNo =>
    """
    Returns the last error code set for this File
    """
    _errno

  fun valid(): Bool =>
    """
    Returns true if the file is currently open.
    """
    not _handle.is_null()

  fun ref line(): String iso^ ? =>
    """
    Returns a line as a String. The newline is not included in the string. If
    there is no more data, this raises an error.
    """
    if _handle.is_null() then
      error
    end

    var offset: U64 = 0
    var len = _last_line_length
    var result = recover String end
    var done = false

    while not done do
      result.reserve(len)

      let r = if Platform.linux() then
        @fgets_unlocked[Pointer[U8]](
          result.cstring().u64() + offset, len - offset, _handle
          )
      else
        @fgets[Pointer[U8]](
          result.cstring().u64() + offset, len - offset, _handle
          )
      end

      result.recalc()

      done = try
        r.is_null() or (result.at_offset(-1) == '\n')
      else
        true
      end

      if not done then
        offset = result.size()
        len = len * 2
      end
    end

    if result.size() == 0 then
      error
    end

    try
      if result.at_offset(-1) == '\n' then
        result.truncate(result.size() - 1)

        if result.at_offset(-1) == '\r' then
          result.truncate(result.size() - 1)
        end
      end
    end

    _last_line_length = len
    result

  fun ref read(len: U64): Array[U8] iso^ =>
    """
    Returns up to len bytes.
    """
    if not _handle.is_null() then
      let result = recover Array[U8].undefined(len) end

      let r = if Platform.linux() then
        @fread_unlocked[U64](result.cstring(), U64(1), len, _handle)
      else
        @fread[U64](result.cstring(), U64(1), len, _handle)
      end

      result.truncate(r)
      result
    else
      recover Array[U8] end
    end

  fun ref read_string(len: U64): String iso^ =>
    """
    Returns up to len bytes. The resulting string may have internal null
    characters.
    """
    if not _handle.is_null() then
      let result = recover String(len) end

      let r = if Platform.linux() then
        @fread_unlocked[U64](result.cstring(), U64(1), len, _handle)
      else
        @fread[U64](result.cstring(), U64(1), len, _handle)
      end

      result.truncate(r)
      result
    else
      recover String end
    end

  fun ref print(data: ByteSeq box): Bool =>
    """
    Same as write, buts adds a newline.
    """
    write(data) and write("\n")

  fun ref printv(data: ByteSeqIter box): Bool =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      if not print(bytes) then
        return false
      end
    end
    true

  fun ref write(data: ByteSeq box): Bool =>
    """
    Returns false if the file wasn't opened with write permission.
    Returns false and closes the file if not all the bytes were written.
    """
    if writeable and (not _handle.is_null()) then
      let len = if Platform.linux() then
        @fwrite_unlocked[U64](data.cstring(), U64(1), data.size(), _handle)
      else
        @fwrite[U64](data.cstring(), U64(1), data.size(), _handle)
      end

      if len == data.size() then
        return true
      end

      dispose()
    end
    false

  fun ref writev(data: ByteSeqIter box): Bool =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      if not write(bytes) then
        return false
      end
    end
    true

  fun position(): U64 =>
    """
    Return the current cursor position in the file.
    """
    if not _handle.is_null() then
      if Platform.windows() then
        @_ftelli64[U64](_handle)
      else
        @ftell[U64](_handle)
      end
    else
      0
    end

  fun ref size(): U64 =>
    """
    Return the total length of the file.
    """
    let pos = position()
    _seek(0, 2)
    let len = position()
    _seek(pos.i64(), 0)
    len

  fun ref seek_start(offset: U64): File =>
    """
    Set the cursor position relative to the start of the file.
    """
    if path.caps(FileSeek) then
      _seek(offset.i64(), 0)
    end
    this

  fun ref seek_end(offset: U64): File =>
    """
    Set the cursor position relative to the end of the file.
    """
    if path.caps(FileSeek) then
      _seek(-offset.i64(), 2)
    end
    this

  fun ref seek(offset: I64): File =>
    """
    Move the cursor position.
    """
    if path.caps(FileSeek) then
      _seek(offset, 1)
    end
    this

  fun ref flush(): File =>
    """
    Flush the file.
    """
    if not _handle.is_null() then
      if Platform.linux() then
        @fflush_unlocked[I32](_handle)
      else
        @fflush[I32](_handle)
      end
    end
    this

  fun ref sync(): File =>
    """
    Sync the file contents to physical storage.
    """
    if path.caps(FileSync) and not _handle.is_null() then
      if Platform.windows() then
        let h = @_get_osfhandle[U64](_fd)
        @FlushFileBuffers[I32](h)
      else
        @fsync[I32](_fd)
      end
    end
    this

  fun ref set_length(len: U64): Bool =>
    """
    Change the file size. If it is made larger, the new contents are undefined.
    """
    if path.caps(FileTruncate) and writeable and (not _handle.is_null()) then
      flush()
      let pos = position()
      let success = if Platform.windows() then
        @_chsize_s[I32](_fd, len) == 0
      else
        @ftruncate[I32](_fd, len) == 0
      end

      if pos >= len then
        _seek(0, 2)
      end
      success
    end
    false

  fun info(): FileInfo ? =>
    """
    Return a FileInfo for this directory. Raise an error if the fd is invalid
    or if we don't have FileStat permission.
    """
    FileInfo._descriptor(_fd, path)

  fun chmod(mode: FileMode box): Bool =>
    """
    Set the FileMode for this directory.
    """
    _FileDes.chmod(_fd, path, mode)

  fun chown(uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for this directory. Does nothing on Windows.
    """
    _FileDes.chown(_fd, path, uid, gid)

  fun touch(): Bool =>
    """
    Set the last access and modification times of the directory to now.
    """
    _FileDes.touch(_fd, path)

  fun set_time(atime: (I64, I64), mtime: (I64, I64)): Bool =>
    """
    Set the last access and modification times of the directory to the given
    values.
    """
    _FileDes.set_time(_fd, path, atime, mtime)

  fun ref lines(): FileLines =>
    """
    Returns an iterator for reading lines from the file.
    """
    FileLines(this)

  fun ref dispose() =>
    """
    Close the file. Future operations will do nothing.
    """
    if not _handle.is_null() then
      @fclose[I32](_handle)
      _handle = Pointer[_FileHandle]
      _fd = -1
    end

  fun ref _seek(offset: I64, base: I32) =>
    """
    Move the cursor position.
    """
    if not _handle.is_null() then
      if Platform.windows() then
        @_fseeki64[I32](_handle, offset, base)
      else
        @fseek[I32](_handle, offset, base)
      end
    end

  fun tag _get_fd(handle: Pointer[_FileHandle]): I32 =>
    """
    Get the file descriptor associated with the file handle.
    """
    if not handle.is_null() then
      if Platform.windows() then
        @_fileno[I32](handle)
      else
        @fileno[I32](handle)
      end
    else
      -1
    end

  fun _final() =>
    """
    Close the file.
    """
    if not _handle.is_null() then
      @fclose[I32](_handle)
    end

class FileLines is Iterator[String]
  """
  Iterate over the lines in a file.
  """
  var _file: File
  var _line: String = ""
  var _next: Bool = false

  new create(file: File) =>
    _file = file

    try
      _line = file.line()
      _next = true
    end

  fun ref has_next(): Bool =>
    _next

  fun ref next(): String =>
    let r = _line

    try
      _line = _file.line()
    else
      _next = false
    end

    r
