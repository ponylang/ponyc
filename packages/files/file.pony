primitive _FileHandle

primitive FileOK
primitive FileError
primitive FileEOF
primitive FileBadFileNumber
primitive FileExists
primitive FilePermissionDenied

primitive _EBADF
  fun apply(): I32 => 9

primitive _EEXIST
  fun apply(): I32 => 17

primitive _EACCES
  fun apply(): I32 => 13
    
type FileErrNo is
  ( FileOK
  | FileError
  | FileEOF
  | FileBadFileNumber
  | FileExists
  | FilePermissionDenied
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
  var _last_line_length: USize = 256
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
        _handle = @fopen[Pointer[_FileHandle]](
          path.path.null_terminated().cstring(), mode.cstring())

        if _handle.is_null() then
          _errno = _get_error()
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

    if
      not from.caps(FileRead) or
      try
        let info' = FileInfo(from)
        info'.directory or info'.pipe
      else
        true
      end
    then
      _errno = FileError
    else
      _handle = @fopen[Pointer[_FileHandle]](
        from.path.null_terminated().cstring(), "rb".cstring())

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
      _errno = _get_error()
      error
    end

    _FileDes.set_rights(_fd, path, writeable)

  fun errno(): FileErrNo =>
    """
    Returns the last error code set for this File
    """
    _errno

  fun ref clear_errno() =>
    """
    Clears the last error code set for this File.
    Clears the error indicator for the stream.
    """
    if not _handle.is_null() then
      @clearerr[None](_handle)
    end
    _errno = FileOK

  fun get_stream_error(): FileErrNo =>
    """
    Checks for errors after File stream operations.
    Retrieves errno if ferror is true.
    """
    if @feof[I32](_handle) != 0 then
      return FileEOF
    end
    if @ferror[I32](_handle) != 0 then
      return _get_error()
    end
    FileOK

  fun _get_error(): FileErrNo =>
    """
    Fetch errno from the OS.
    """
    let os_errno = @pony_os_errno[I32]()
    match os_errno
    | _EBADF() => return FileBadFileNumber
    | _EEXIST() => return FileExists
    | _EACCES() => return FilePermissionDenied
    else
      return FileError
    end

    
  fun valid(): Bool =>
    """
    Returns true if the file is currently open.
    """
    not _handle.is_null()

  fun get_fd(): I32 ? =>
    """
    Returns the underlying file descriptor.
    Raises an error if the file is not currently open.
    """
    if _handle.is_null() then
      error
    end

    _fd

  fun ref line(): String iso^ ? =>
    """
    Returns a line as a String. The newline is not included in the string. If
    there is no more data, this raises an error.
    """
    if _handle.is_null() then
      error
    end

    var offset: USize = 0
    var len = _last_line_length
    var result = recover String end
    var done = false

    while not done do
      result.reserve(len)

      let r = ifdef linux then
        @fgets_unlocked[Pointer[U8]](result.cstring().usize() + offset,
          result.space() - offset, _handle)
      else
        @fgets[Pointer[U8]](result.cstring().usize() + offset,
          result.space() - offset, _handle)
      end

      result.recalc()

      if r.is_null() then
        _errno = get_stream_error() // either EOF or error
      end
      
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

  fun ref read(len: USize): Array[U8] iso^ =>
    """
    Returns up to len bytes.
    """
    if not _handle.is_null() then
      let result = recover Array[U8].undefined(len) end

      let r = ifdef linux then
        @fread_unlocked[USize](result.cstring(), USize(1), len, _handle)
      else
        @fread[USize](result.cstring(), USize(1), len, _handle)
      end

      if r < len then
        _errno = get_stream_error() // EOF or error
      end 
      
      result.truncate(r)
      result
    else
      recover Array[U8] end
    end

  fun ref read_string(len: USize): String iso^ =>
    """
    Returns up to len bytes. The resulting string may have internal null
    characters.
    """
    if not _handle.is_null() then
      let result = recover String(len) end

      let r = ifdef linux then
        @fread_unlocked[USize](result.cstring(), USize(1), result.space(),
          _handle)
      else
        @fread[USize](result.cstring(), USize(1), result.space(), _handle)
      end

      if r < len then
        _errno = get_stream_error() // EOF or error
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
      let len = ifdef linux then
        @fwrite_unlocked[USize](data.cstring(), USize(1), data.size(), _handle)
      else
        @fwrite[USize](data.cstring(), USize(1), data.size(), _handle)
      end

      if len == data.size() then
        return true
      end
      // check error
      _errno = get_stream_error()
      // fwrite can't resume in case of error
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

  fun ref position(): USize =>
    """
    Return the current cursor position in the file.
    """
    if not _handle.is_null() then
      let r = ifdef windows then
        @_ftelli64[U64](_handle).usize()
      else
        @ftell[USize](_handle)
      end
      if r < 0 then
        _errno = _get_error()
      end
      r
    else
      0
    end

  fun ref size(): USize =>
    """
    Return the total length of the file.
    """
    let pos = position()
    _seek(0, 2)
    let len = position()
    _seek(pos.isize(), 0)
    len

  fun ref seek_start(offset: USize): File =>
    """
    Set the cursor position relative to the start of the file.
    """
    if path.caps(FileSeek) then
      _seek(offset.isize(), 0)
    end
    this

  fun ref seek_end(offset: USize): File =>
    """
    Set the cursor position relative to the end of the file.
    """
    if path.caps(FileSeek) then
      _seek(-offset.isize(), 2)
    end
    this

  fun ref seek(offset: ISize): File =>
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
      let r = ifdef linux then
        @fflush_unlocked[I32](_handle)
      else
        @fflush[I32](_handle)
      end
      if r != 0 then
        _errno = _get_error()
      end
    end
    this

  fun ref sync(): File =>
    """
    Sync the file contents to physical storage.
    """
    if path.caps(FileSync) and not _handle.is_null() then
      ifdef windows then
        let h = @_get_osfhandle[U64](_fd)
        @FlushFileBuffers[I32](h)
      else
        let r = @fsync[I32](_fd)
        if r < 0 then
          _errno = _get_error()
        end
      end
    end
    this

  fun ref set_length(len: USize): Bool =>
    """
    Change the file size. If it is made larger, the new contents are undefined.
    """
    if path.caps(FileTruncate) and writeable and (not _handle.is_null()) then
      flush()
      let pos = position()
      let result = ifdef windows then
        @_chsize_s[I32](_fd, len)
      else
        @ftruncate[I32](_fd, len)
      end

      if pos >= len then
        _seek(0, 2)
      end
      
      if result == 0 then
        true
      else
        _errno = _get_error()
        false
      end
    else
      false
    end

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

  fun ref _seek(offset: ISize, base: I32) =>
    """
    Move the cursor position.
    """
    if not _handle.is_null() then
      ifdef windows then
        @_fseeki64[I32](_handle, offset, base)
      else
        let r = @fseek[I32](_handle, offset, base)
        if r < 0 then
          _errno = _get_error()
        end
      end
    end

  fun tag _get_fd(handle: Pointer[_FileHandle]): I32 =>
    """
    Get the file descriptor associated with the file handle.
    """
    if not handle.is_null() then
      ifdef windows then
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
