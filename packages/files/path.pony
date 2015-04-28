use "time"

primitive _PathSep
primitive _PathDot
primitive _PathDot2
primitive _PathOther

type _PathState is (_PathSep | _PathDot | _PathDot2 | _PathOther)

primitive Path
  """
  Operations on paths that do not require an open file or directory.
  """

  fun chmod(path: String, mode: FileMode box): Bool =>
    """
    Set the FileMode for a path.
    """
    var m = mode._os()

    if Platform.windows() then
      @_chmod[I32](path.cstring(), m) == 0
    else
      @chmod[I32](path.cstring(), m) == 0
    end

  fun chown(path: String, uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for a path. Does nothing on Windows.
    """
    if Platform.windows() then
      false
    else
      @chown[I32](path.cstring(), uid, gid) == 0
    end

  fun touch(path: String): Bool =>
    """
    Set the last access and modification times of a path to now.
    """
    set_time(path, Time.now(), Time.now())

  fun set_time(path: String, atime: (I64, I64), mtime: (I64, I64)): Bool =>
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

  fun is_sep(c: U8): Bool =>
    """
    Determine if a byte is a path separator.
    """
    (c == '/') or (Platform.windows() and (c == '\\'))

  fun sep(): String =>
    """
    Return the path separator as a string.
    """
    if Platform.windows() then "\\" else "/" end

  fun is_abs(path: String): Bool =>
    """
    Return true if the path is an absolute path.
    """
    try
      if Platform.windows() then
        is_sep(path(0)) or _drive_letter(path)
      else
        is_sep(path(0))
      end
    else
      false
    end

  fun join(path: String, next_path: String): String =>
    """
    Join two paths together. If the next_path is absolute, simply return it.
    The returned path will be cleaned.
    """
    if path.size() == 0 then
      clean(next_path)
    elseif next_path.size() == 0 then
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

  fun clean(path: String): String =>
    """
    Replace multiple separators with a single separator.
    Remove instances of . from the path.
    Remove instances of .. and the preceding path element from the path.
    The result will have no trailing slash unless it is a root directory.
    If the result would be empty, "." will be returned instead.
    """
    var s = recover String(path.size()) end
    var vol = volume(path)
    s.append(vol)

    var state: _PathState = _PathOther
    var i = vol.size()
    var backtrack = I64(-1)
    let n = path.size()

    try
      var c = path(i)

      if is_sep(c) then
        s.append(sep())
        i = i + 1
        state = _PathSep
      elseif c == '.' then
        i = i + 1
        state = _PathDot
      else
        backtrack = s.size().i64()
      end

      while i < n do
        c = path(i)

        if is_sep(c) then
          match state
          | _PathDot2 =>
            if backtrack == -1 then
              s.append("..")
              s.append(sep())
            else
              s.delete(backtrack, -1)

              try
                backtrack = s.rfind(sep()) + 1
              else
                backtrack = vol.size().i64()
              end

              if (s.size() == 0) or (s.compare("../", 3, backtrack) == 0) then
                backtrack = -1
              end
            end
          | _PathOther =>
            s.append(sep())
          end
          state = _PathSep
        elseif c == '.' then
          match state
          | _PathSep =>
            state = _PathDot
          | _PathDot =>
            state = _PathDot2
          | _PathDot2 =>
            backtrack = s.size().i64()
            s.append("...")
            state = _PathOther
          | _PathOther =>
            s.append(".")
          end
        else
          match state
          | _PathSep =>
            backtrack = s.size().i64()
          | _PathDot =>
            backtrack = s.size().i64()
            s.append(".")
          | _PathDot2 =>
            backtrack = s.size().i64()
            s.append("..")
          end
          s.push(c)
          state = _PathOther
        end

        i = i + 1
      end
    end

    match state
    | _PathDot2 =>
      if backtrack == -1 then
        s.append("..")
      else
        s.delete(backtrack, -1)
      end
    end

    try
      if is_sep(s(-1)) then
        s.delete(-1, 1)
      end
    end

    if s.size() > 0 then
      s
    else
      "."
    end

  fun cwd(): String =>
    """
    Returns the program's working directory. Setting the working directory is
    not supported, as it is not concurrency-safe.
    """
    recover String.from_cstring(@os_cwd[Pointer[U8]](), 0, false) end

  fun abs(path: String): String =>
    """
    Returns a cleaned, absolute path.
    """
    if is_abs(path) then
      clean(path)
    else
      join(cwd(), path)
    end

  fun rel(to: String, target: String): String ? =>
    """
    Returns a path such that Path.join(to, Path.rel(to, target)) == target.
    Raises an error if this isn't possible.
    """
    var to_clean = clean(to)
    var target_clean = clean(target)

    if to_clean == target_clean then
      return "."
    end

    var to_i: I64 = 0

    if Platform.windows() then
      to_clean = abs(to_clean)
      target_clean = abs(target_clean)

      var to_vol = volume(to_clean)
      var target_vol = volume(target_clean)

      if to_vol != target_vol then
        error
      end

      to_i = to_vol.size().i64()
    end

    var to_0 = to_i
    var target_i = to_i
    var target_0 = target_i

    while true do
      to_i = try
        to_clean.find(sep(), to_i)
      else
        to_clean.size().i64()
      end

      target_i = try
        target_clean.find(sep(), target_i)
      else
        target_clean.size().i64()
      end

      if
        (to_i != target_i) or
        (to_clean.compare(target_clean, target_i.u64()) != 0)
      then
        break
      end

      if to_i < to_clean.size().i64() then
        to_i = to_i + 1
      end

      if target_i < target_clean.size().i64() then
        target_i = target_i + 1
      end

      to_0 = to_i
      target_0 = target_i
    end

    if
      ((to_i - to_0) == 2) and
      (to_clean.compare("..", 2, to_0) == 0)
    then
      error
    end

    if to_0.u64() != to_clean.size() then
      var result = recover String end

      try
        while true do
          to_i = to_clean.find(sep(), to_i) + 1
          result.append("..")
          result.append(sep())
        end
      end

      result.append("..")
      result.append(sep())
      result.append(target_clean.substring(target_0, -1))
      result
    else
      target_clean.substring(target_0, -1)
    end

  fun base(path: String): String =>
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

  fun dir(path: String): String =>
    """
    Return a cleaned path before the last separator, or the whole path if there
    is no separator.
    """
    try
      var i = path.rfind(sep())
      clean(path.substring(0, i - 1))
    else
      path
    end

  fun ext(path: String): String =>
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
    ""

  fun volume(path: String): String =>
    """
    On Windows, this returns the drive letter or UNC base at the beginning of
    the path, if there is one. Otherwise, this returns an empty string.
    """
    if Platform.windows() then
      var offset = I64(0)

      if path.compare("""\\?\""", 4) == 0 then
        offset = 4

        if path.compare("""UNC\""", 4, offset) == 0 then
          return _network_share(path, offset + 4)
        end
      end

      if _drive_letter(path, offset) then
        return path.substring(0, offset + 1)
      end

      try
        if
          is_sep(path.at_offset(offset)) and
          is_sep(path.at_offset(offset + 1))
        then
          return _network_share(path, offset + 2)
        end
      end
    end
    ""

  fun _drive_letter(path: String, offset: I64 = 0): Bool =>
    """
    Look for a drive letter followed by a ':', returning true if we find it.
    """
    try
      var c = path.at_offset(offset)

      (((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z'))) and
        (path.at_offset(offset + 1) == ':')
    else
      false
    end

  fun _network_share(path: String, offset: I64 = 0): String =>
    """
    Look for a host, a \, and a resource. Return the path up to that point if
    we found one, otherwise an empty String.
    """
    try
      var next = path.find("\\", offset) + 1

      try
        next = path.find("\\", next) - 1
        path.substring(0, next)
      else
        path
      end
    else
      ""
    end

  fun from_slash(path: String): String =>
    """
    Changes each / in the path to the OS specific separator.
    """
    if Platform.windows() then
      var s = path.clone()
      var len = s.size()
      var i = U64(0)

      try
        while i < len do
          if s(i) == '/' then
            s(i) = '\\'
          end

          i = i + 1
        end
      end

      s
    else
      path
    end

  fun to_slash(path: String): String =>
    """
    Changes each OS specific separator in the path to /.
    """
    if Platform.windows() then
      var s = path.clone()
      var len = s.size()
      var i = U64(0)

      try
        while i < len do
          if s(i) == '\\' then
            s(i) = '/'
          end

          i = i + 1
        end
      end

      s
    else
      path
    end

  fun exists(path: String): Bool =>
    """
    Returns true if the path exists. Returns false for a broken symlink.
    """
    try
      not FileInfo(path).broken
    else
      false
    end

  fun mkdir(path: String): Bool =>
    """
    Creates the directory. Will recursively create each element. Returns true
    if the directory exists when we're done, false if it does not.
    """
    var offset: I64 = 0

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
        @mkdir[I32](element.cstring(), U32(0x1FF))
      end
    until offset < 0 end

    exists(path)

  fun remove(path: String): Bool =>
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

  fun rename(path: String, new_path: String): Bool =>
    """
    Rename a file or directory.
    """
    @rename[I32](path.cstring(), new_path.cstring()) == 0

  fun symlink(target: String, link_name: String): Bool =>
    """
    Create a symlink to a file or directory.
    """
    if Platform.windows() then
      @CreateSymbolicLink[U8](link_name.cstring(), target.cstring()) != 0
    else
      @symlink[I32](target.cstring(), link_name.cstring()) == 0
    end

  fun canonical(path: String): String ? =>
    """
    Return the equivalent canonical absolute path. Raise an error if there
    isn't one.
    """
    var cstring = @os_realpath[Pointer[U8] iso^](path.cstring())

    if cstring.is_null() then
      error
    else
      recover String.from_cstring(consume cstring, 0, false) end
    end

  fun is_list_sep(c: U8): Bool =>
    """
    Determine if a byte is a path list separator.
    """
    if Platform.windows() then c == ';' else c == ':' end

  fun list_sep(): String =>
    """
    Return the path list separator as a string.
    """
    if Platform.windows() then ";" else ":" end

  fun split_list(path: String): Array[String] iso^ =>
    """
    Separate a list of paths into an array of cleaned paths.
    """
    var array = recover Array[String] end
    var offset: I64 = 0

    try
      while true do
        var next = path.find(list_sep(), offset)
        array.push(clean(path.substring(offset, next - 1)))
        offset = next + 1
      end
    else
      array.push(clean(path.substring(offset, -1)))
    end

    array
