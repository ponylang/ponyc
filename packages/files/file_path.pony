use "time"

use @_mkdir[I32](dir: Pointer[U8] tag) if windows
use @mkdir[I32](path: Pointer[U8] tag, mode: U32) if not windows
use @_rmdir[I32](path: Pointer[U8] tag) if windows
use @rmdir[I32](path: Pointer[U8] tag) if not windows
use @_unlink[I32](path: Pointer [U8] tag) if windows
use @unlink[I32](path: Pointer [U8] tag) if not windows
use @rename[I32](old_path: Pointer[U8] tag, new_path: Pointer[U8] tag)
use @pony_os_eexist[I32]()
use @CreateSymbolicLinkA[U8](src_name: Pointer[U8] tag,
  target_name: Pointer[U8] tag, flags: U32) if windows
use @symlink[I32](path: Pointer[U8] tag, path2: Pointer[U8] tag)
  if not windows
use @_chmod[I32](path: Pointer[U8] tag, mode: U32)
  if windows
use @chmod[I32](path: Pointer[U8] tag, mode: U32)
  if not windows
use @chown[I32](path: Pointer[U8] tag, uid: U32, gid: U32)
  if not windows
use @utimes[I32](path: Pointer[U8] tag,
  tv: Pointer[(ILong, ILong, ILong, ILong)]) if not windows
use @_utime64[I32](path: Pointer[U8] tag, times: Pointer[(I64, I64)])
  if windows
use @LookupPrivilegeValueA[U8](system_name: Pointer[None] tag,
  priv_name: Pointer[U8] tag, luid: Pointer[U64]) if windows
use @GetCurrentProcess[USize]() if windows
use @OpenProcessToken[U8](handle: Pointer[None] tag, access: U32,
  token_handle: Pointer[None] tag) if windows
use @AdjustTokenPrivileges[U8](handle: Pointer[None], disable_privileges: U8,
  new_state: _TokenPrivileges, buf_len: U32, prev_state: Pointer[None],
  ret_len: Pointer[None]) if windows
use @CloseHandle[Bool](handle: Pointer[None]) if windows

interface WalkHandler
  """
  A handler for `FilePath.walk`.
  """
  fun ref apply(dir_path: FilePath, dir_entries: Array[String] ref)

class val FilePath
  """
  A FilePath represents a capability to access a path. The path will be
  represented as an absolute path and a set of capabilities for operations on
  that path.
  """
  let path: String
    """
    Absolute filesystem path.
    """
  let caps: FileCaps = FileCaps
    """
    Set of capabilities for operations on `path`.
    """

  new val create(
    base: FileAuth,
    path': String,
    caps': FileCaps val = recover val FileCaps .> all() end)
  =>
    """
    Create a new path to any location.

    Unless absolute, path' will be relative to the program's working directory.

    Capabilities are exactly as given.
    """
    caps.union(caps')
    path = Path.abs(path')

  new val from(
    base: FilePath,
    path': String,
    caps': FileCaps val = recover val FileCaps .> all() end) ?
  =>
    """
    Create a new path from an existing `FilePath`.

    path' is relative to the existing `FilePath`,
    and the existing `FilePath` must be a prefix of the resulting path.

    The resulting `FilePath` will have capabilities that are the intersection of
    the supplied capabilities and the capabilities of the existing `FilePath`.
    """
    caps.union(caps')
    if not base.caps(FileLookup) then
      error
    end

    let tmp_path = Path.join(base.path, path')
    caps.intersect(base.caps)

    if not tmp_path.at(base.path, 0) then
      error
    end
    path = tmp_path

  new val mkdtemp(
    base: (FileAuth | FilePath),
    prefix: String = "",
    caps': FileCaps val = recover val FileCaps .> all() end) ?
  =>
    """
    Create a temporary directory and returns a path to it. The directory's name
    will begin with `prefix`.

    If `FileAuth` is provided, the resulting `FilePath` will
    be relative to the program's working directory. Otherwise, it will be
    relative to the existing `FilePath`, and the existing `FilePath` must be a
    prefix of the resulting path.

    The resulting `FilePath` will have capabilities that are the intersection
    of the supplied capabilities and the capabilities on the base.
    """
    (let dir, let pre) = Path.split(prefix)
    let parent: FilePath = match \exhaustive\ base
      | let base': FileAuth => FilePath(base', dir)
      | let base': FilePath => FilePath.from(base', dir)?
    end

    if not parent.mkdir() then
      error
    end

    var temp = FilePath.from(parent, pre + Path.random())?

    while not temp.mkdir(true) do
      temp = FilePath.from(parent, pre + Path.random())?
    end

    caps.union(caps')
    caps.intersect(temp.caps)
    path = temp.path

  new val _create(path': String, caps': FileCaps val) =>
    """
    Internal constructor.
    """
    path = path'
    caps.union(caps')

  fun val join(
    path': String,
    caps': FileCaps val = recover val FileCaps .> all() end)
    : FilePath ?
  =>
    """
    Return a new path relative to this one.
    """
    from(this, path', caps')?

  fun val walk(handler: WalkHandler ref, follow_links: Bool = false) =>
    """
    Walks a directory structure starting at this.

    `handler(dir_path, dir_entries)` will be called for each directory
    starting with this one. The handler can control which subdirectories are
    expanded by removing them from the `dir_entries` list.
    """
    try
      with dir = Directory(this)? do
        var entries: Array[String] ref = dir.entries()?
        handler(this, entries)
        for e in entries.values() do
          let p = this.join(e)?
          let info = FileInfo(p)?
          if info.directory and (follow_links or not info.symlink) then
            p.walk(handler, follow_links)
          end
        end
      end
    else
      return
    end

  fun val canonical(): FilePath ? =>
    """
    Return the equivalent canonical absolute path. Raise an error if there
    isn't one.
    """
    _create(Path.canonical(path)?, caps)

  fun val exists(): Bool =>
    """
    Returns true if the path exists. Returns false for a broken symlink.
    """
    try
      not FileInfo(this)?.broken
    else
      false
    end

  fun val mkdir(must_create: Bool = false): Bool =>
    """
    Creates the directory. Will recursively create each element. Returns true
    if the directory exists when we're done, false if it does not. If we do not
    have the FileStat permission, this will return false even if the directory
    does exist.
    """
    if not caps(FileMkdir) then
      return false
    end

    var offset: ISize = 0

    repeat
      let element = try
        offset = path.find(Path.sep(), offset)? + 1
        path.substring(0, offset - 1)
      else
        offset = -1
        path
      end

      if element.size() > 0 then
        let r = ifdef windows then
          @_mkdir(element.cstring())
        else
          @mkdir(element.cstring(), U32(0x1FF))
        end

        if r != 0 then
          if @pony_os_errno() != @pony_os_eexist() then
            return false
          end

          if must_create and (offset < 0) then
            return false
          end
        end
      end
    until offset < 0 end

    try
      FileInfo(this)?.directory
    else
      false
    end

  fun val remove(): Bool =>
    """
    Remove the file or directory. The directory contents will be removed as
    well, recursively. Symlinks will be removed but not traversed.
    """
    if not caps(FileRemove) then
      return false
    end

    try
      let info = FileInfo(this)?

      if info.directory and not info.symlink then
        let directory = Directory(this)?

        for entry in directory.entries()?.values() do
          if not join(entry)?.remove() then
            return false
          end
        end
      end

      ifdef windows then
        // _unlink() on Windows can't remove readonly files
        // so we change mode to _S_IWRITE just in case
        @_chmod(path.cstring(), U32(0x0080))
        if info.directory and not info.symlink then
          0 == @_rmdir(path.cstring())
        else
          0 == @_unlink(path.cstring())
        end
      else
        if info.directory and not info.symlink then
          0 == @rmdir(path.cstring())
        else
          0 == @unlink(path.cstring())
        end
      end
    else
      false
    end

  fun rename(new_path: FilePath): Bool =>
    """
    Rename a file or directory.
    """
    if not caps(FileRename) or not new_path.caps(FileCreate) then
      return false
    end

    0 == @rename(path.cstring(), new_path.path.cstring())

  fun val symlink(link_name: FilePath): Bool =>
    """
    Create a symlink to a file or directory.

    Note that on Windows a program must be running with elevated priviledges to
    be able to create symlinks.
    """
    if not caps(FileLink) or not link_name.caps(FileCreate) then
      return false
    end

    ifdef windows then
      try
        var err: U32 = 0

        // look up the ID of the SE_CREATE_SYMBOLIC_LINK privilege
        let priv_name = "SeCreateSymbolicLinkPrivilege"
        var luid: U64 = 0
        var ret = @LookupPrivilegeValueA(
          USize(0),
          priv_name.cstring(),
          addressof luid)
        if ret == 0 then
          return false
        end

        // get current process pseudo-handle
        let handle = @GetCurrentProcess()
        if handle == 0 then
          return false
        end

        // get security token
        var token: USize = 0
        let rights: U32 = 0x0020 // TOKEN_ADJUST_PRIVILEGES
        ret = @OpenProcessToken(handle, rights, addressof token)
        if ret == 0 then
          return false
        end

        // try to turn on SE_CREATE_SYMBOLIC_LINK privilege
        // this won't work unless we are administrator or have Developer Mode on
        // https://blogs.windows.com/windowsdeveloper/2016/12/02/symlinks-windows-10/
        var privileges: _TokenPrivileges ref = _TokenPrivileges
        privileges.privilege_count = 1
        privileges.privileges_0.luid = luid
        privileges.privileges_0.attributes = 0x00000002 // SE_PRIVILEGE_ENABLED
        ret = @AdjustTokenPrivileges(token, U8(0), privileges,
          U32(0), USize(0), USize(0))
        @CloseHandle(token)
        if ret == 0 then
          return false
        end

        // now actually try to create the link
        let flags: U32 = if FileInfo(this)?.directory then 3 else 0 end
        ret = @CreateSymbolicLinkA(link_name.path.cstring(),
          path.cstring(), flags)
        ret != 0
      else
        false
      end
    else
      0 == @symlink(path.cstring(), link_name.path.cstring())
    end

  fun chmod(mode: FileMode box): Bool =>
    """
    Set the FileMode for a path.
    """
    if not caps(FileChmod) then
      return false
    end

    let m = mode.u32()

    ifdef windows then
      0 == @_chmod(path.cstring(), m)
    else
      0 == @chmod(path.cstring(), m)
    end

  fun chown(uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a path. Does nothing on Windows.
    """
    ifdef windows then
      false
    else
      if caps(FileChown) then
        0 == @chown(path.cstring(), uid, gid)
      else
        false
      end
    end

  fun touch(): Bool =>
    """
    Set the last access and modification times of a path to now.
    """
    set_time(Time.now(), Time.now())

  fun set_time(atime: (I64, I64), mtime: (I64, I64)): Bool =>
    """
    Set the last access and modification times of a path to the given values.
    """
    if not caps(FileTime) then
      return false
    end

    ifdef windows then
      var tv: (I64, I64) = (atime._1, mtime._1)
      0 == @_utime64(path.cstring(), addressof tv)
    else
      var tv: (ILong, ILong, ILong, ILong) =
        ( atime._1.ilong(), atime._2.ilong() / 1000,
          mtime._1.ilong(), mtime._2.ilong() / 1000 )

      0 == @utimes(path.cstring(), addressof tv)
    end

struct ref _TokenPrivileges
  var privilege_count: U32 = 1
  embed privileges_0: _LuidAndAttributes = _LuidAndAttributes

struct ref _LuidAndAttributes
  var luid: U64 = 0
  var attributes: U32 = 0
