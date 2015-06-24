use "time"

class FilePath val
  """
  A FilePath represents a capability to access a path. The path will be
  represented as an absolute path. If the FilePath is marked as readonly, it
  can only be used to create further readonly capabilities.
  """
  let path: String
  let readonly: Bool

  new val create(base: (FilePath | Root | None), path': String,
    readonly': Bool = false) ?
  =>
    """
    Create a new path. The caller must either provide the root capability or an
    existing FilePath.

    If the root capability is provided, path' will be relative to the program's
    working directory. Otherwise, it will be relative to the existing FilePath,
    and the existing FilePath must be a prefix of the resulting path.

    The resulting FilePath will be readonly if either the readonly parameter
    is true or the base FilePath is readonly.
    """
    match base
    | let b: FilePath =>
      path = Path.join(b.path, path')
      readonly = readonly' or b.readonly

      if not path.at(b.path, 0) then
        error
      end
    | let b: Root =>
      path = Path.abs(path')
      readonly = readonly'
    else
      error
    end

  new val _create(path': String, readonly': Bool) =>
    """
    Internal constructor.
    """
    path = path'
    readonly = readonly'

  fun val join(path': String, readonly': Bool = false): FilePath ? =>
    """
    Return a new path relative to this one.
    """
    create(this, path', readonly')

  fun val canonical(): FilePath ? =>
    """
    Return the equivalent canonical absolute path. Raise an error if there
    isn't one.
    """
    _create(Path.canonical(path), readonly)

  fun val exists(): Bool =>
    """
    Returns true if the path exists. Returns false for a broken symlink.
    """
    try
      not FileInfo(this).broken
    else
      false
    end

  fun val mkdir(): Bool =>
    """
    Creates the directory. Will recursively create each element. Returns true
    if the directory exists when we're done, false if it does not.
    """
    var offset: I64 = 0

    repeat
      var element = try
        offset = path.find(Path.sep(), offset) + 1
        path.substring(0, offset - 2)
      else
        offset = -1
        path
      end

      if Platform.windows() then
        @_mkdir[I32](element.cstring())
      else
        @mkdir[I32](element.cstring(), U32(0x1FF))
      end
    until offset < 0 end

    exists()

  fun val remove(): Bool =>
    """
    Remove the file or directory. The directory contents will be removed as
    well, recursively. Symlinks will be removed but not traversed.
    """
    try
      var info = FileInfo(this)

      if info.directory and not info.symlink then
        var directory = Directory(this)

        for entry in directory.entries.values() do
          if not join(entry).remove() then
            return false
          end
        end
      end

      if Platform.windows() then
        if info.directory and not info.symlink then
          @_rmdir[I32](path.cstring()) == 0
        else
          @_unlink[I32](path.cstring()) == 0
        end
      else
        if info.directory and not info.symlink then
          @rmdir[I32](path.cstring()) == 0
        else
          @unlink[I32](path.cstring()) == 0
        end
      end
    else
      false
    end

  fun rename(new_path: FilePath): Bool =>
    """
    Rename a file or directory.
    """
    @rename[I32](path.cstring(), new_path.path.cstring()) == 0

  fun symlink(link_name: FilePath): Bool =>
    """
    Create a symlink to a file or directory.
    """
    if Platform.windows() then
      @CreateSymbolicLink[U8](link_name.path.cstring(), path.cstring()) != 0
    else
      @symlink[I32](path.cstring(), link_name.path.cstring()) == 0
    end

  fun chmod(mode: FileMode box): Bool =>
    """
    Set the FileMode for a path.
    """
    if readonly then
      return false
    end

    var m = mode._os()

    if Platform.windows() then
      @_chmod[I32](path.cstring(), m) == 0
    else
      @chmod[I32](path.cstring(), m) == 0
    end

  fun chown(uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a path. Does nothing on Windows.
    """
    if readonly or Platform.windows() then
      false
    else
      @chown[I32](path.cstring(), uid, gid) == 0
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
    if readonly then
      return false
    end

    if Platform.windows() then
      var tv: (I64, I64) = (atime._1, mtime._1)
      @_utime64[I32](path.cstring(), tv) == 0
    else
      var tv: (I64, I64, I64, I64) =
        (atime._1, atime._2 / 1000, mtime._1, mtime._2 / 1000)
      @utimes[I32](path.cstring(), tv) == 0
    end
