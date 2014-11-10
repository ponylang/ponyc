primitive Path
  """
  Operations on paths that do not require an open file or directory.
  """

  fun tag chmod(path: String, mode: FileMode box): Bool =>
    """
    Set the FileMode for a path.
    """
    var m = mode._os()

    if Platform.windows() then
      @_chmod[I32](path.cstring(), m) == 0
    else
      @chmod[I32](path.cstring(), m) == 0
    end

  fun tag chown(path: String, uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a path. Does nothing on Windows.
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
    """
    Returns a cleaned, absolute path.
    """
    if is_abs(path) then
      clean(path)
    else
      clean(join(cwd(), path))
    end

  fun tag rel(base: String, path: String): String =>
    // TODO:
    path

  fun tag base(path: String): String =>
    """
    Return the path after the last separator, or the whole path if there is no
    separator.
    """
    try
      var i = path.rfind(sep())
      path.substring(i + 1, -1)
    else
      path
    end

  fun tag dir(path: String): String =>
    """
    Return the path before the last separator, or the whole path if there is no
    separator.
    """
    try
      var i = path.rfind(sep())
      path.substring(0, i - 1)
    else
      path
    end

  fun tag ext(path: String): String =>
    """
    Return the file extension, i.e. the part after the last dot as long as that
    dot is after all separators. Return an empty string for no extension.
    """
    try
      var i = path.rfind(".")

      var j = try
        path.rfind(sep())
      else
        i
      end

      if i >= j then
        return path.substring(i + 1, -1)
      end
    end
    recover String end

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
    """
    Changes each / in the path to the OS specific separator.
    """
    if Platform.windows() then
      var s = path.clone()
      var len = s.length().i64()
      var i: I64 = 0

      try
        while i < len do
          if s(i) == '/' then
            s(i) = sep()(0)
          end

          i = i + 1
        end
      end

      consume s
    else
      path
    end

  fun tag to_slash(path: String): String =>
    """
    Changes each OS specific separator in the path to /.
    """
    if Platform.windows() then
      var s = path.clone()
      var len = s.length().i64()
      var i: I64 = 0

      try
        while i < len do
          if s(i) == sep()(0) then
            s(i) = '/'
          end

          i = i + 1
        end
      end

      consume s
    else
      path
    end

  fun tag exists(path: String): Bool =>
    """
    Returns true if the path exists. Returns false for a broken symlink.
    """
    try
      not FileInfo(path).broken
    else
      false
    end

  fun tag mkdir(path: String, mode: FileMode box): Bool =>
    """
    Creates the directory. Will recursively create each element. Returns true
    if the directory exists when we're done, false if it does not. On Windows,
    the mode is ignored.
    """
    var offset: I64 = 0
    var m = mode._os()

    repeat
      var element = try
        offset = path.find(sep(), offset) + 1
        path.substring(0, offset - 2)
      else
        offset = -1
        path
      end

      if Platform.windows() then
        @_mkdir[I32](element.cstring())
      else
        @mkdir[I32](element.cstring(), m)
      end
    until offset < 0 end

    try
      FileInfo(path)
      true
    else
      false
    end

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
      @CreateSymbolicLink[U8](link_name.cstring(), target.cstring()) != 0
    else
      @symlink[I32](target.cstring(), link_name.cstring()) == 0
    end

  fun tag readlink(path: String): String =>
    // TODO:
    path

  fun tag realpath(path: String): String =>
    // TODO:
    path
