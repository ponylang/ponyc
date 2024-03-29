use "time"

use @ponyint_o_rdonly[I32]()
use @ponyint_o_rdwr[I32]()
use @ponyint_o_creat[I32]()
use @ponyint_o_trunc[I32]()
use @ponyint_o_directory[I32]()
use @ponyint_o_cloexec[I32]()
use @ponyint_at_removedir[I32]()
use @openat[I32](fd: I32, path: Pointer[U8] tag, flags: I32, ...)
  if linux or bsd
use @unlinkat[I32](fd: I32, target: Pointer[U8] tag, flags: I32)
use @futimes[I32](fildes: I32, tv: Pointer[(ILong, ILong, ILong, ILong)])
  if not windows
use @renameat[I32](fd: I32, from: Pointer[U8] tag, tofd: I32, to_path: Pointer[U8] tag)
  if linux or bsd
use @symlinkat[I32](source: Pointer[U8] tag, fd: I32, dst: Pointer[U8] tag) if linux or bsd
use @futimesat[I32](fd: I32, path: Pointer[U8] tag,
 timeval: Pointer[(ILong, ILong, ILong, ILong)]) if linux or bsd
use @fchownat[I32](fd: I32, path: Pointer[U8] tag, uid: U32, gid: U32, flags: I32) if linux or bsd
use @fchmodat[I32](fd: I32, path: Pointer[U8] tag, mode: U32, flag: I32) if linux or bsd
use @mkdirat[I32](fd: I32, path: Pointer[U8] tag, mode: U32)
use @fdopendir[Pointer[_DirectoryHandle]](fd: I32) if posix
use @opendir[Pointer[_DirectoryHandle]](name: Pointer[U8] tag) if posix
use @closedir[I32](dir: Pointer[_DirectoryHandle] tag) if posix
use @ponyint_unix_readdir[Pointer[U8] iso^](dir: Pointer[None] tag)
use @ponyint_windows_find_data[Pointer[_DirectoryEntry]]()
  if windows
use @ponyint_windows_find_data_free[None](data: Pointer[_DirectoryEntry])
  if windows
use @ponyint_windows_readdir[Pointer[U8] iso^](data: Pointer[_DirectoryEntry])
  if windows
use @FindFirstFileA[Pointer[_DirectoryHandle]](file_name: Pointer[U8] tag, data: Pointer[_DirectoryEntry])
  if windows
use @FindNextFileA[Bool](handle: Pointer[None] tag, data: Pointer[_DirectoryEntry])
  if windows
use @FindClose[Bool](handle: Pointer[_DirectoryHandle] tag) if windows

primitive _DirectoryHandle
primitive _DirectoryEntry

class Directory
  """
  Operations on a directory.

  The directory-relative functions (open, etc) use the *at interface on FreeBSD
  and Linux. This isn't available on OS X prior to 10.10, so it is not used. On
  FreeBSD, this allows the directory-relative functions to take advantage of
  Capsicum.
  """
  let path: FilePath
    """
    This is the filesystem path locating this directory on the file system
    and an object capability granting access to operate on this directory.
    """

  var _fd: I32 = -1
  // We don't need a file descriptor in Windows. However we do still need to
  // know whether we've disposed of this object, so we use the _fd to indicate
  // this.
  // 0 => not yet disposed of.
  // -1 => disposed of.

  new create(from: FilePath) ? =>
    """
    This will raise an error if the path doesn't exist or it is not a
    directory, or if FileRead or FileStat permission isn't available.
    """
    if not from.caps(FileRead) then
      error
    end

    if not FileInfo(from)?.directory then
      error
    end

    path = from

    ifdef posix then
      _fd =
        @open(from.path.cstring(),
          @ponyint_o_rdonly()
            or @ponyint_o_directory()
            or @ponyint_o_cloexec())

      if _fd == -1 then
        error
      end
    elseif windows then
      _fd = 0
    else
      compile_error "unsupported platform"
    end

    _FileDes.set_rights(_fd, path)?

  new iso _relative(path': FilePath, fd': I32) =>
    """
    Internal constructor. Capsicum rights are already set by inheritence.
    """
    path = path'
    _fd = fd'

  fun entries(): Array[String] iso^ ? =>
    """
    The entries will include everything in the directory, but it is not
    recursive. The path for the entry will be relative to the directory, so it
    will contain no directory separators. The entries will not include "." or
    "..".
    """
    if not path.caps(FileRead) or (_fd == -1) then
      error
    end

    let path' = path.path
    let fd' = _fd

    recover
      let list = Array[String]

      ifdef posix then
        if fd' == -1 then
          error
        end

        let h =
          ifdef linux or bsd then
            let fd =
              @openat(fd', ".".cstring(),
                @ponyint_o_rdonly()
                  or @ponyint_o_directory()
                  or @ponyint_o_cloexec())
            @fdopendir(fd)
          else
            @opendir(path'.cstring())
          end

        if h.is_null() then
          error
        end

        while true do
          let p = @ponyint_unix_readdir(h)
          if p.is_null() then break end
          list.push(recover String.from_cstring(consume p) end)
        end

        @closedir(h)
      elseif windows then
        var find = @ponyint_windows_find_data()
        let search = path' + "\\*"
        let h = @FindFirstFileA(search.cstring(), find)

        if h.usize() == -1 then
          error
        end

        repeat
          let p = @ponyint_windows_readdir(find)

          if not p.is_null() then
            list.push(recover String.from_cstring(consume p) end)
          end
        until not @FindNextFileA(h, find) end

        @FindClose(h)
        @ponyint_windows_find_data_free(find)
      else
        compile_error "unsupported platform"
      end

      consume list
    end

  fun open(target: String): Directory iso^ ? =>
    """
    Open a directory relative to this one. Raises an error if the path is not
    within this directory hierarchy.
    """
    if _fd == -1 then
      error
    end

    let path' = FilePath.from(path, target, path.caps)?

    ifdef linux or bsd then
      let fd' =
        @openat(_fd, target.cstring(),
          @ponyint_o_rdonly()
            or @ponyint_o_directory()
            or @ponyint_o_cloexec())
      _relative(path', fd')
    else
      recover create(path')? end
    end

  fun mkdir(target: String): Bool =>
    """
    Creates a directory relative to this one. Returns false if the path is
    not within this directory hierarchy or if FileMkdir permission is missing.
    """
    if
      not path.caps(FileMkdir) or
      not path.caps(FileLookup) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, target, path.caps)?

      ifdef linux or bsd then
        var offset: ISize = 0

        repeat
          let element = try
            offset = target.find(Path.sep(), offset)? + 1
            target.substring(0, offset - 1)
          else
            offset = -1
            target
          end

          @mkdirat(_fd, element.cstring(), U32(0x1FF))
        until offset < 0 end

        FileInfo(path')?.directory
      else
        path'.mkdir()
      end
    else
      false
    end

  fun create_file(target: String): File iso^ ? =>
    """
    Open for read/write, creating if it doesn't exist, preserving the contents
    if it does exist.
    """
    if
      not path.caps(FileCreate)
        or not path.caps(FileRead)
        or not path.caps(FileWrite)
        or (_fd == -1)
    then
      error
    end

    let path' = FilePath.from(path, target, path.caps)?

    ifdef linux or bsd then
      let fd' =
        @openat(_fd, target.cstring(),
          @ponyint_o_rdwr()
            or @ponyint_o_creat()
            or @ponyint_o_cloexec(),
          I32(0x1B6))
      recover File._descriptor(fd', path')? end
    else
      recover File(path') end
    end

  fun open_file(target: String): File iso^ ? =>
    """
    Open for read only, failing if it doesn't exist.
    """
    if
      not path.caps(FileRead)
        or (_fd == -1)
    then
      error
    end

    let path' = FilePath.from(path, target, path.caps - FileWrite)?

    ifdef linux or bsd then
      let fd' =
        @openat(_fd, target.cstring(),
          @ponyint_o_rdonly() or @ponyint_o_cloexec(),
          I32(0x1B6))
      recover File._descriptor(fd', path')? end
    else
      recover File.open(path') end
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

  fun infoat(target: String): FileInfo ? =>
    """
    Return a FileInfo for some path relative to this directory.
    """
    if
      not path.caps(FileStat)
        or not path.caps(FileLookup)
        or (_fd == -1)
    then
      error
    end

    let path' = FilePath.from(path, target, path.caps)?

    ifdef linux or bsd then
      FileInfo._relative(_fd, path', target)?
    else
      FileInfo(path')?
    end

  fun chmodat(target: String, mode: FileMode box): Bool =>
    """
    Set the FileMode for some path relative to this directory.
    """
    if
      not path.caps(FileChmod)
        or not path.caps(FileLookup)
        or (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, target, path.caps)?

      ifdef linux or bsd then
        0 == @fchmodat(_fd, target.cstring(), mode.u32(), I32(0))
      else
        path'.chmod(mode)
      end
    else
      false
    end

  fun chownat(target: String, uid: U32, gid: U32): Bool =>
    """
    Set the FileMode for some path relative to this directory.
    """
    if
      not path.caps(FileChown)
        or not path.caps(FileLookup)
        or (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, target, path.caps)?

      ifdef linux or bsd then
        0 == @fchownat(_fd, target.cstring(), uid, gid, I32(0))
      else
        path'.chown(uid, gid)
      end
    else
      false
    end

  fun touchat(target: String): Bool =>
    """
    Set the last access and modification times of the directory to now.
    """
    set_time_at(target, Time.now(), Time.now())

  fun set_time_at(
    target: String,
    atime: (I64, I64),
    mtime: (I64, I64))
    : Bool
  =>
    """
    Set the last access and modification times of the directory to the given
    values.
    """
    if
      not path.caps(FileChown)
        or not path.caps(FileLookup)
        or (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, target, path.caps)?

      ifdef linux or bsd then
        var tv: (ILong, ILong, ILong, ILong) =
          ( atime._1.ilong(), atime._2.ilong() / 1000,
            mtime._1.ilong(), mtime._2.ilong() / 1000 )

        0 == @futimesat(_fd, target.cstring(), addressof tv)
      else
        path'.set_time(atime, mtime)
      end
    else
      false
    end

  fun symlink(source: FilePath, link_name: String): Bool =>
    """
    Link the source path to the link_name, where the link_name is relative to
    this directory.
    """
    if
      not path.caps(FileLink)
        or not path.caps(FileLookup)
        or not path.caps(FileCreate)
        or not source.caps(FileLink)
        or (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, link_name, path.caps)?

      ifdef linux or bsd then
        0 == @symlinkat(source.path.cstring(), _fd, link_name.cstring())
      else
        source.symlink(path')
      end
    else
      false
    end

  fun remove(target: String): Bool =>
    """
    Remove the file or directory. The directory contents will be removed as
    well, recursively. Symlinks will be removed but not traversed.
    """
    if
      not path.caps(FileLookup)
        or not path.caps(FileRemove)
        or (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, target, path.caps)?

      ifdef linux or bsd then
        let fi = FileInfo(path')?

        if fi.directory and not fi.symlink then
          let directory = open(target)?

          for entry in directory.entries()?.values() do
            if not directory.remove(entry) then
              return false
            end
          end

          0 == @unlinkat(_fd, target.cstring(), @ponyint_at_removedir())
        else
          0 == @unlinkat(_fd, target.cstring(), 0)
        end
      else
        path'.remove()
      end
    else
      false
    end

  fun rename(source: String, to: Directory box, target: String): Bool =>
    """
    Rename source (which is relative to this directory) to target (which is
    relative to the `to` directory).
    """
    if
      not path.caps(FileLookup)
       or not path.caps(FileRename)
       or not to.path.caps(FileLookup)
       or not to.path.caps(FileCreate)
       or (_fd == -1)
       or (to._fd == -1)
    then
      return false
    end

    try
      let path' = FilePath.from(path, source, path.caps)?
      let path'' = FilePath.from(to.path, target, to.path.caps)?

      ifdef linux or bsd then
        0 == @renameat(_fd, source.cstring(), to._fd, target.cstring())
      else
        path'.rename(path'')
      end
    else
      false
    end

  fun ref dispose() =>
    """
    Close the directory.
    """
    if _fd != -1 then
      ifdef posix then
        @close(_fd)
      end

      _fd = -1
    end

  fun _final() =>
    """
    Close the file descriptor.
    """
    if _fd != -1 then
      ifdef posix then
        @close(_fd)
      end
    end
