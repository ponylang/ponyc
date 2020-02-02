use "time"

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
    base: (FilePath | AmbientAuth),
    path': String,
    caps': FileCaps val = recover val FileCaps .> all() end)
    ?
  =>
    """
    Create a new path. The caller must either provide the root capability or an
    existing FilePath.

    If the root capability is provided, path' will be relative to the program's
    working directory. Otherwise, it will be relative to the existing FilePath,
    and the existing FilePath must be a prefix of the resulting path.

    The resulting FilePath will have capabilities that are the intersection of
    the supplied capabilities and the capabilities on the parent.
    """
    caps.union(caps')

    path = match base
      | let b: FilePath =>
        if not b.caps(FileLookup) then
          error
        end

        let tmp_path = Path.join(b.path, path')
        caps.intersect(b.caps)

        if not tmp_path.at(b.path, 0) then
          error
        end
        tmp_path
      | let b: AmbientAuth =>
        Path.abs(path')
      end

  new val mkdtemp(
    base: (FilePath | AmbientAuth),
    prefix: String = "",
    caps': FileCaps val = recover val FileCaps .> all() end)
    ?
  =>
    """
    Create a temporary directory and returns a path to it. The directory's name
    will begin with `prefix`. The caller must either provide the root
    capability or an existing FilePath.

    If AmbientAuth is provided, pattern will be relative to the program's
    working directory. Otherwise, it will be relative to the existing
    FilePath, and the existing FilePath must be a prefix of the resulting path.

    The resulting FilePath will have capabilities that are the intersection of
    the supplied capabilities and the capabilities on the base.
    """
    (let dir, let pre) = Path.split(prefix)
    let parent = FilePath(base, dir)?

    if not parent.mkdir() then
      error
    end

    var temp = FilePath(parent, pre + Path.random())?

    while not temp.mkdir(true) do
      temp = FilePath(parent, pre + Path.random())?
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
    create(this, path', caps')?

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
          @_mkdir[I32](element.cstring())
        else
          @mkdir[I32](element.cstring(), U32(0x1FF))
        end

        if r != 0 then
          if @pony_os_errno[I32]() != @pony_os_eexist[I32]() then
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
        if info.directory and not info.symlink then
          0 == @_rmdir[I32](path.cstring())
        else
          0 == @_unlink[I32](path.cstring())
        end
      else
        if info.directory and not info.symlink then
          0 == @rmdir[I32](path.cstring())
        else
          0 == @unlink[I32](path.cstring())
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

    0 == @rename[I32](path.cstring(), new_path.path.cstring())

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
        var ret = @LookupPrivilegeValueA[U8](
          USize(0),
          priv_name.cstring(),
          addressof luid)
        if ret == 0 then
          return false
        end

        // get current process pseudo-handle
        let handle = @GetCurrentProcess[USize]()
        if handle == 0 then
          return false
        end

        // get security token
        var token: USize = 0
        let rights: U32 = 0x0020 // TOKEN_ADJUST_PRIVILEGES
        ret = @OpenProcessToken[U8](handle, rights, addressof token)
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
        ret = @AdjustTokenPrivileges[U8](
          token,
          U8(0),
          privileges,
          U32(0),
          USize(0),
          USize(0))
        @CloseHandle[U8](token)
        if ret == 0 then
          return false
        end

        // now actually try to create the link
        let flags: U32 = if FileInfo(this)?.directory then 3 else 0 end
        ret = @CreateSymbolicLinkA[U8](
          link_name.path.cstring(),
          path.cstring(),
          flags)
        ret != 0
      else
        false
      end
    else
      0 == @symlink[I32](path.cstring(), link_name.path.cstring())
    end

  fun chmod(mode: FileMode box): Bool =>
    """
    Set the FileMode for a path.
    """
    if not caps(FileChmod) then
      return false
    end

    let m = mode._os()

    ifdef windows then
      0 == @_chmod[I32](path.cstring(), m)
    else
      0 == @chmod[I32](path.cstring(), m)
    end

  fun chown(uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a path. Does nothing on Windows.
    """
    ifdef windows then
      false
    else
      if caps(FileChown) then
        0 == @chown[I32](path.cstring(), uid, gid)
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
      0 == @_utime64[I32](path.cstring(), addressof tv)
    else
      var tv: (ILong, ILong, ILong, ILong) =
        ( atime._1.ilong(), atime._2.ilong() / 1000,
          mtime._1.ilong(), mtime._2.ilong() / 1000 )

      0 == @utimes[I32](path.cstring(), addressof tv)
    end

struct ref _TokenPrivileges
  var privilege_count: U32 = 1
  embed privileges_0: _LuidAndAttributes = _LuidAndAttributes

struct ref _LuidAndAttributes
  var luid: U64 = 0
  var attributes: U32 = 0
