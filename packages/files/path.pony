primitive Path
  """
  Operations on paths that do not require an open file or directory.
  """

  fun tag chmod(path: String, mode: FileMode box): Bool =>
    """
    Set the FileMode for a file or directory.
    """
    var m: U32 = 0

    if Platform.windows() then
      if mode.owner_read or mode.group_read or mode.any_read then
        m = m or 0x100
      end

      if mode.owner_write or mode.group_write or mode.any_write then
        m = m or 0x80
      end

      @_chmod[I32](path.cstring(), m) == 0
    else
      if mode.setuid then m = m or 0x800 end
      if mode.setgid then m = m or 0x400 end
      if mode.sticky then m = m or 0x200 end
      if mode.owner_read then m = m or 0x100 end
      if mode.owner_write then m = m or 0x80 end
      if mode.owner_exec then m = m or 0x40 end
      if mode.group_read then m = m or 0x20 end
      if mode.group_write then m = m or 0x10 end
      if mode.group_exec then m = m or 0x8 end
      if mode.any_read then m = m or 0x4 end
      if mode.any_write then m = m or 0x2 end
      if mode.any_exec then m = m or 0x1 end
      @chmod[I32](path.cstring(), m) == 0
    end

  fun tag chown(path: String, uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a file or directory.
    """
    if Platform.windows() then
      false
    else
      @chown[I32](path.cstring(), uid, gid) == 0
    end

  fun tag touch(path: String): Bool =>
    """
    Set the last access and modification times of a path to now.
    """
    set_time(path, Time.now(), Time.now())

  fun tag set_time(path: String, atime: (I64, I64), mtime: (I64, I64)): Bool =>
    """
    Set the last access and modification times of a path to the given values.
    """
    if Platform.windows() then
      var tv: (I64, I64) = (atime._1, mtime._1)
      @_utime64[I32](path.cstring(), tv) == 0
    else
      var tv: (I64, I64, I64, I64) =
        (atime._1, atime._2 / 1000, mtime._1, mtime._2 / 1000)
      @utimes[I32](path.cstring(), tv) == 0
    end

  fun tag is_sep(c: U8): Bool =>
    """
    Determine if a byte is a path separator.
    """
    (c == '/') or (Platform.windows() and (c == '\\'))

  fun tag sep(): String =>
    """
    Return the path separator as a string.
    """
    if Platform.windows() then "\\" else "/" end

  fun tag is_list_sep(c: U8): Bool =>
    """
    Determine if a byte is a list separator.
    """
    if Platform.windows() then c == ';' else c == ':' end

  fun tag list_sep(): String =>
    """
    Return the list separator as a string.
    """
    if Platform.windows() then ";" else ":" end

  fun tag is_abs(path: String): Bool =>
    """
    Return true if the path is an absolute path.
    """
    try
      if Platform.windows() then
        var c = path(0)
        is_sep(c) or
        ((c >= 'A') and (c <= 'Z') and (path(1) == ':') and is_sep(path(2)))
      else
        is_sep(path(0))
      end
    else
      false
    end

  fun tag join(path: String, next_path: String): String =>
    """
    Join two paths together. If the next_path is absolute, simply return it.
    Paths have Path.clean() called on them before being returned.
    """
    if path.length() == 0 then
      clean(next_path)
    elseif next_path.length() == 0 then
      clean(path)
    elseif is_abs(next_path) then
      clean(next_path)
    else
      try
        if is_sep(path(-1)) then
          if is_sep(next_path(0)) then
            return clean(path + next_path.substring(1, -1))
          else
            return clean(path + next_path)
          end
        end
      end
      clean(path + sep() + next_path)
    end

  fun tag clean(path: String): String =>
    // TODO:
    path

  fun tag cwd(): String iso^ =>
    """
    Returns the program's working directory. Setting the working directory is
    not supported, as it is not concurrency-safe.
    """
    recover String.from_cstring(@os_cwd[Pointer[U8]]()) end

  fun tag abs(path: String): String =>
    if is_abs(path) then
      clean(path)
    else
      clean(join(cwd(), path))
    end

  fun tag rel(base: String, path: String): String =>
    // TODO:
    path

  fun tag base(path: String): String =>
    // TODO:
    path

  fun tag dir(path: String): String =>
    // TODO:
    path

  fun tag ext(path: String): String =>
    // TODO:
    path

  fun tag volume(path: String): String =>
    """
    On Windows, this returns the drive letter at the beginning of the path,
    if there is one. Otherwise, this returns an empty string.
    """
    if Platform.windows() then
      try
        var c = path(0)
        if (c >= 'A') and (c <= 'Z') and (path(1) == ':') then
          return path.substring(0, 0)
        end
      end
    end
    recover String end

  fun tag from_slash(path: String): String =>
    // TODO:
    path

  fun tag to_slash(path: String): String =>
    // TODO:
    path

  fun tag exists(path: String): Bool =>
    """
    Returns true if the path exists. Returns false for a broken symlink.
    """
    try
      FileInfo(path)
      true
    else
      false
    end

  fun tag mkdir(path: String): Bool =>
    // TODO:
    false

  fun tag remove(path: String): Bool =>
    """
    Remove the file or directory. The directory contents will be removed as
    well, recursively. Symlinks will be removed but not traversed.
    """
    try
      var info = FileInfo(path)

      if info.directory and not info.symlink then
        var directory = Directory(path)

        for entry in directory.entries.values() do
          if not remove(join(path, entry)) then
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

  fun tag rename(path: String, new_path: String): Bool =>
    """
    Rename a file or directory.
    """
    @rename[I32](path.cstring(), new_path.cstring()) == 0

  fun tag symlink(target: String, link_name: String): Bool =>
    """
    Create a symlink to a file or directory.
    """
    if Platform.windows() then
      @CreateSymbolicLink[I32](link_name.cstring(), target.cstring()) != 0
    else
      @symlink[I32](target.cstring(), link_name.cstring()) == 0
    end

  fun tag readlink(path: String): String =>
    // TODO:
    path

  fun tag realpath(path: String): String =>
    // TODO:
    path
