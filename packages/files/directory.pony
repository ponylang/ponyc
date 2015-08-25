use "time"

use @o_rdonly[I32]()
use @o_rdwr[I32]()
use @o_creat[I32]()
use @o_trunc[I32]()
use @o_directory[I32]()
use @o_cloexec[I32]()
use @at_removedir[I32]()
use @unlinkat[I32](fd: I32, target: Pointer[U8] tag, flags: I32)

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
  var _fd: I32 = -1

  new create(from: FilePath) ? =>
    """
    This will raise an error if the path doesn't exist or it is not a
    directory, or if FileRead or FileStat permission isn't available.
    """
    if not from.caps(FileRead) then
      error
    end

    if not FileInfo(from).directory then
      error
    end

    path = from

    if Platform.posix() then
      _fd = @open[I32](from.path.cstring(),
        @o_rdonly() or @o_directory() or @o_cloexec())

      if _fd == -1 then
        error
      end
    end

    _FileDes.set_rights(_fd, path)

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

      if Platform.windows() then
        var find = @windows_find_data[Pointer[_DirectoryEntry]]()
        let search = path' + "\\*"
        let h = @FindFirstFile[Pointer[_DirectoryHandle]](
          search.cstring(), find)

        if h.u64() == -1 then
          error
        end

        repeat
          let p = @windows_readdir[Pointer[U8] iso^](find)

          if not p.is_null() then
            list.push(recover String.from_cstring(consume p) end)
          end
        until not @FindNextFile[Bool](h, find) end

        @FindClose[Bool](h)
        @free[None](find)
      else
        if fd' == -1 then
          error
        end

        let fd = @openat[I32](fd', ".".cstring(),
          @o_rdonly() or @o_directory() or @o_cloexec())
        let h = @fdopendir[Pointer[_DirectoryHandle]](fd)

        if h.is_null() then
          error
        end

        while true do
          let p = @unix_readdir[Pointer[U8] iso^](h)
          if p.is_null() then break end
          list.push(recover String.from_cstring(consume p) end)
        end

        @closedir[I32](h)
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

    let path' = FilePath(path, target, path.caps)

    if Platform.windows() or Platform.osx() then
      recover create(path') end
    else
      let fd' = @openat[I32](_fd, target.cstring(),
        @o_rdonly() or @o_directory() or @o_cloexec())
      _relative(path', fd')
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
      let path' = FilePath(path, target, path.caps)

      if Platform.windows() or Platform.osx() then
        path'.mkdir()
      else
        var offset: I64 = 0

        repeat
          let element = try
            offset = target.find(Path.sep(), offset) + 1
            target.substring(0, offset - 2)
          else
            offset = -1
            target
          end

          @mkdirat[I32](_fd, element.cstring(), U32(0x1FF))
        until offset < 0 end

        FileInfo(path').directory
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
      not path.caps(FileCreate) or
      not path.caps(FileRead) or
      not path.caps(FileWrite) or
      (_fd == -1)
    then
      error
    end

    let path' = FilePath(path, target, path.caps)

    if Platform.windows() or Platform.osx() then
      recover File(path') end
    else
      let fd' = @openat[I32](_fd, target.cstring(),
        @o_rdwr() or @o_creat() or @o_cloexec(), I32(0x1B6))
      recover File._descriptor(fd', path') end
    end

  fun open_file(target: String): File iso^ ? =>
    """
    Open for read only, failing if it doesn't exist.
    """
    if
      not path.caps(FileRead) or
      (_fd == -1)
    then
      error
    end

    let path' = FilePath(path, target, path.caps - FileWrite)

    if Platform.windows() or Platform.osx() then
      recover File(path') end
    else
      let fd' = @openat[I32](_fd, target.cstring(),
        @o_rdonly() or @o_cloexec(), I32(0x1B6))
      recover File._descriptor(fd', path') end
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

  fun infoat(target: String): FileInfo ? =>
    """
    Return a FileInfo for some path relative to this directory.
    """
    if
      not path.caps(FileStat) or
      not path.caps(FileLookup) or
      (_fd == -1)
    then
      error
    end

    let path' = FilePath(path, target, path.caps)

    if Platform.windows() or Platform.osx() then
      FileInfo(path')
    else
      FileInfo._relative(_fd, path', target)
    end

  fun chmodat(target: String, mode: FileMode box): Bool =>
    """
    Set the FileMode for some path relative to this directory.
    """
    if
      not path.caps(FileChmod) or
      not path.caps(FileLookup) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, target, path.caps)

      if Platform.windows() or Platform.osx() then
        path'.chmod(mode)
      else
        @fchmodat[I32](_fd, target.cstring(), mode._os(), I32(0)) == 0
      end
    else
      false
    end

  fun chownat(target: String, uid: U32, gid: U32): Bool =>
    """
    Set the FileMode for some path relative to this directory.
    """
    if
      not path.caps(FileChown) or
      not path.caps(FileLookup) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, target, path.caps)

      if Platform.windows() or Platform.osx() then
        path'.chown(uid, gid)
      else
        @fchownat[I32](_fd, target.cstring(), uid, gid, I32(0)) == 0
      end
    else
      false
    end

  fun touchat(target: String): Bool =>
    """
    Set the last access and modification times of the directory to now.
    """
    set_time_at(target, Time.now(), Time.now())

  fun set_time_at(target: String, atime: (I64, I64), mtime: (I64, I64)): Bool
  =>
    """
    Set the last access and modification times of the directory to the given
    values.
    """
    if
      not path.caps(FileChown) or
      not path.caps(FileLookup) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, target, path.caps)

      if Platform.windows() or Platform.osx() then
        path'.set_time(atime, mtime)
      else
        var tv: (I64, I64, I64, I64) =
          (atime._1, atime._2 / 1000, mtime._1, mtime._2 / 1000)
        @futimesat[I32](_fd, target.cstring(), &tv) == 0
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
      not path.caps(FileLink) or
      not path.caps(FileLookup) or
      not path.caps(FileCreate) or
      not source.caps(FileLink) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, link_name, path.caps)

      if Platform.windows() or Platform.osx() then
        source.symlink(path')
      else
        @symlinkat[I32](source.path.cstring(), _fd, link_name.cstring()) == 0
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
      not path.caps(FileLookup) or
      not path.caps(FileRemove) or
      (_fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, target, path.caps)

      if Platform.windows() or Platform.osx() then
        path'.remove()
      else
        let fi = FileInfo(path')

        if fi.directory and not fi.symlink then
          let directory = open(target)

          for entry in directory.entries().values() do
            if not directory.remove(entry) then
              return false
            end
          end

          @unlinkat(_fd, target.cstring(), @at_removedir()) == 0
        else
          @unlinkat(_fd, target.cstring(), 0) == 0
        end
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
      not path.caps(FileLookup) or
      not path.caps(FileRename) or
      not to.path.caps(FileLookup) or
      not to.path.caps(FileCreate) or
      (_fd == -1) or (to._fd == -1)
    then
      return false
    end

    try
      let path' = FilePath(path, source, path.caps)
      let path'' = FilePath(to.path, target, to.path.caps)

      if Platform.windows() or Platform.osx() then
        path'.rename(path'')
      else
        @renameat[I32](_fd, source.cstring(), to._fd, target.cstring()) == 0
      end
    else
      false
    end

  fun ref dispose() =>
    """
    Close the directory.
    """
    if Platform.posix() then
      @close[I32](_fd)
      _fd = -1
    end

  fun _final() =>
    """
    Close the file descriptor.
    """
    if _fd != -1 then
      @close[I32](_fd)
    end
