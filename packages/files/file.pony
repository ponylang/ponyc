use @_open[I32](path: Pointer[U8] tag, flags: I32, ...) if windows
use @open[I32](path: Pointer[U8] tag, flags: I32, ...) if not windows
use @_read[I32](fd: I32, buffer: Pointer[None], bytes_to_read: I32) if windows
use @read[ISize](fd: I32, buffer: Pointer[None], bytes_to_read: USize)
  if not windows
use @_write[I32](fd: I32, buffer: Pointer[None], bytes_to_send: I32) if windows
use @writev[ISize](fd: I32, buffer: Pointer[None], num_to_send: I32)
  if not windows
use @pony_os_lseek[I64](fd: I32, offset: I64, base: I32)
use @FlushFileBuffers[Bool](file_handle: Pointer[None]) if windows
use @_get_osfhandle[Pointer[None]](fd: I32) if windows
use @fsync[I32](fd: I32) if not windows
use @fdatasync[I32](fd: I32) if not windows
use @pony_os_ftruncate[I32](fd: I32, len: I64)
use @_close[I32](fd: I32) if windows
use @close[I32](fd: I32) if not windows
use @pony_os_writev_max[I32]()
use @pony_os_errno[I32]()

use "collections"

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
    """
    This is the filesystem path locating this file on the file system
    and an object capability granting access to operate on this file.
    """

  let writeable: Bool
    """
    `true` if the underlying file descriptor has been opened as writeable.
    """

  let _newline: String = "\n"
  var _unsynced_data: Bool = false
  var _unsynced_metadata: Bool = false
  var _fd: I32
  var _errno: FileErrNo = FileOK
  embed _pending_writev: Array[(Pointer[U8] tag, USize)] = _pending_writev.create()
  var _pending_writev_total: USize = 0

  new create(from: FilePath) =>
    """
    Attempt to open for read/write, creating if it doesn't exist, preserving
    the contents if it does exist.
    Set errno according to result.
    """
    path = from
    writeable = true
    _fd = -1

    if not from.caps(FileRead) or not from.caps(FileWrite) then
      _errno = FileError
    else
      var flags: I32 = @ponyint_o_rdwr()
      let mode = FileMode.u32() // default file permissions
      if not path.exists() then
        if not path.caps(FileCreate) then
          _errno = FileError
        else
          flags = flags or @ponyint_o_creat() or @ponyint_o_trunc()
        end
      end

      _fd = ifdef windows then
        @_open(path.path.cstring(), flags, mode.i32())
      else
        @open(path.path.cstring(), flags, mode)
      end

      if _fd == -1 then
        _errno = _get_error()
      else
        try
          _FileDes.set_rights(_fd, path, writeable)?
        else
          _errno = FileError
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

    if
      not path.caps(FileRead) or
      try
        let info' = FileInfo(path)?
        info'.directory or info'.pipe
      else
        true
      end
    then
      _errno = FileError
    else
      _fd = ifdef windows then
        @_open(path.path.cstring(), @ponyint_o_rdonly())
      else
        @open(path.path.cstring(), @ponyint_o_rdonly())
      end

      if _fd == -1 then
        _errno = _get_error()
      else
        try
          _FileDes.set_rights(_fd, path, writeable)?
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
    writeable = from.caps(FileWrite)
    _fd = fd

    _FileDes.set_rights(_fd, path, writeable)?

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
    _errno = FileOK

  fun _get_error(): FileErrNo =>
    """
    Fetch errno from the OS.
    """
    let os_errno = @pony_os_errno()
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
    not (_fd == -1)


  fun ref read(len: USize): Array[U8] iso^ =>
    """
    Returns up to len bytes.
    """
    if _fd != -1 then
      let result = recover Array[U8] .> undefined(len) end

      let r =
        (ifdef windows then
          @_read(_fd, result.cpointer(), len.i32())
        else
          @read(_fd, result.cpointer(), len)
        end)
          .isize()

      match r
      | 0  => _errno = FileEOF
      | -1 => _errno = _get_error()
      end

      result.truncate(r.usize())
      result
    else
      recover Array[U8] end
    end

  fun ref read_string(len: USize): String iso^ =>
    """
    Returns up to len bytes. The resulting string may have internal null
    characters.
    """
    if _fd != -1 then
      let result = recover String(len) end

      let r = (ifdef windows then
        @_read(_fd, result.cpointer(), result.space().i32())
      else
        @read(_fd, result.cpointer(), result.space())
      end).isize()

      match r
      | 0  => _errno = FileEOF
      | -1 => _errno = _get_error()
      end

      result.truncate(r.usize())
      result
    else
      recover String end
    end

  fun ref print(data: ByteSeq box): Bool =>
    """
    Same as write, buts adds a newline.
    """
    queue(data)
    queue(_newline)

    _pending_writes()

  fun ref printv(data: ByteSeqIter box): Bool =>
    """
    Print an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      queue(bytes)
      queue(_newline)
    end

    _pending_writes()

  fun ref write(data: ByteSeq box): Bool =>
    """
    Returns false if the file wasn't opened with write permission.
    Returns false and closes the file if not all the bytes were written.
    """
    queue(data)

    _pending_writes()

  fun ref writev(data: ByteSeqIter box): Bool =>
    """
    Write an iterable collection of ByteSeqs.
    """
    for bytes in data.values() do
      queue(bytes)
    end

    _pending_writes()

  fun ref queue(data: ByteSeq box) =>
    """
    Queue data to be written
    NOTE: Queue'd data will always be written before normal print/write
    requested data
    """
    _pending_writev .> push((data.cpointer(), data.size()))
    _pending_writev_total = _pending_writev_total + data.size()

  fun ref queuev(data: ByteSeqIter box) =>
    """
    Queue an iterable collection of ByteSeqs to be written
    NOTE: Queue'd data will always be written before normal print/write
    requested data
    """
    for bytes in data.values() do
      queue(bytes)
    end

  fun ref flush(): Bool =>
    """
    Flush any queued data
    """
    _pending_writes()

  fun ref _pending_writes(): Bool =>
    """
    Write pending data.
    Returns false if the file wasn't opened with write permission.
    Returns false and closes the file and discards all pending data
    if not all the bytes were written.
    Returns true if it sent all pending data.
    """
    try
      (let result, let num_written, let new_pending_total) =
        _write_to_disk()?
      _pending_writev_total = new_pending_total
      if _pending_writev_total == 0 then
        _pending_writev.clear()
        _unsynced_data = true
        _unsynced_metadata = true
      else
        if num_written > 0 then
          _unsynced_data = true
          _unsynced_metadata = true
        end
        for d in Range[USize](0, num_written, 1) do
          _pending_writev.shift()?
        end
      end
      return result
    else
      // TODO: error recovery? EINTR?

      // check error
      _errno = _get_error()

      dispose()
      return false
    end

  fun _write_to_disk(): (Bool, USize, USize) ? =>
    """
    Write pending data.
    Returns false if the file wasn't opened with write permission.
    Raises an error if not all the bytes were written.
    Returns true if it sent all pending data.
    Returns num_processed and new pending_total also.
    """
    var num_to_send: I32 = 0
    var num_sent: USize = 0
    var bytes_to_send: USize = 0
    var pending_total = _pending_writev_total

    if (not writeable) or (_fd == -1) then
      return (false, num_sent, pending_total)
    end

    // TODO: Make writev_batch_size user configurable
    let writev_batch_size = @pony_os_writev_max()
    while pending_total > 0 do
      // Determine the number of bytes and buffers to send.
      num_to_send = _pending_writev.size().i32() - num_sent.i32()
      if num_to_send <= writev_batch_size then
        bytes_to_send = pending_total
      else
        // We have more buffers than a single writev can handle.
        // We must iterate over the buffers being sent to add up to the total.
        num_to_send = writev_batch_size
        bytes_to_send = 0
        var counter: I32 = num_sent.i32()
        repeat
          bytes_to_send = bytes_to_send + _pending_writev(counter.usize())?._2
          counter = counter + 1
        until counter >= num_to_send end
      end

      // Write as much data as possible (vectored i/o).
      // On Windows only write 1 buffer at a time.
      var len = ifdef windows then
        @_write(_fd, _pending_writev(num_sent)?._1,
          bytes_to_send.i32()).isize()
      else
        @writev(_fd, _pending_writev.cpointer(num_sent),
          num_to_send).isize()
      end

      if len < bytes_to_send.isize() then error end

      // We've sent all the data we requested in this batch.
      pending_total = pending_total - bytes_to_send
      num_sent = num_sent + num_to_send.usize()
    end

    (true, num_sent, pending_total)

  fun ref position(): USize =>
    """
    Return the current cursor position in the file.
    """
    if _fd != -1 then
      let o: I64 = 0
      let b: I32 = 1
      let r = @pony_os_lseek(_fd, o, b)

      if r < 0 then
        _errno = _get_error()
      end
      r.usize()
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
    _seek(pos.i64(), 0)
    len

  fun ref seek_start(offset: USize) =>
    """
    Set the cursor position relative to the start of the file.
    """
    if path.caps(FileSeek) then
      _seek(offset.i64(), 0)
    end

  fun ref seek_end(offset: USize) =>
    """
    Set the cursor position relative to the end of the file.
    """
    if path.caps(FileSeek) then
      _seek(-offset.i64(), 2)
    end

  fun ref seek(offset: ISize) =>
    """
    Move the cursor position.
    """
    if path.caps(FileSeek) then
      _seek(offset.i64(), 1)
    end

  fun ref sync() =>
    """
    Sync the file contents to physical storage.
    """
    if path.caps(FileSync) and (_fd != -1) then
      ifdef windows then
        let r = @FlushFileBuffers(@_get_osfhandle(_fd))
        if r == true then
          _errno = FileError
        end
      else
        let r = @fsync(_fd)
        if r < 0 then
          _errno = _get_error()
        end
      end
    end
    _unsynced_data = false
    _unsynced_metadata = false

  fun ref datasync() =>
    """
    Sync the file contents to physical storage.
    """
    if path.caps(FileSync) and (_fd != -1) then
      ifdef windows then
        let r = @FlushFileBuffers(@_get_osfhandle(_fd))
        if r == true then
          _errno = FileError
        end
      else
        let r = @fdatasync(_fd)
        if r < 0 then
          _errno = _get_error()
        end
      end
    end
    _unsynced_data = false

  fun ref set_length(len: USize): Bool =>
    """
    Change the file size. If it is made larger, the new contents are undefined.
    """
    if path.caps(FileTruncate) and writeable and (_fd != -1) then
      let pos = position()
      let result =  @pony_os_ftruncate(_fd, len.i64())

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
    FileInfo._descriptor(_fd, path)?

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
    if _fd != -1 then
      if (_pending_writev_total > 0) and (_errno is FileOK) then
        flush()
      end
      if _unsynced_data or _unsynced_metadata then
        sync()
      end
      let r = ifdef windows then
        @_close(_fd)
      else
        @close(_fd)
      end
      if r < 0 then
        _errno = _get_error()
      end
      _fd = -1

      _pending_writev_total = 0
      _pending_writev.clear()
    end

  fun ref _seek(offset: I64, base: I32) =>
    """
    Move the cursor position.
    """
    if _fd != -1 then
      let r = @pony_os_lseek(_fd, offset, base)
      if r < 0 then
        _errno = _get_error()
      end
    end

  fun _final() =>
    """
    Close the file.
    """
    if _fd != -1 then
      if (_pending_writev_total > 0) and (_errno is FileOK) then
        // attempt to write any buffered data
        try
          _write_to_disk()?
        end
      end
      if _unsynced_data or _unsynced_metadata then
        // attempt to sync any un-synced data
        if (path.caps.value() and FileSync.value()) > 0 then
          ifdef windows then
            @FlushFileBuffers(@_get_osfhandle(_fd))
          else
            @fsync(_fd)
          end
        end
      end
      // close file
      ifdef windows then
        @_close(_fd)
      else
        @close(_fd)
      end
    end
