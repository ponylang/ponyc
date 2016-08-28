use "time"

primitive _PathSep
primitive _PathDot
primitive _PathDot2
primitive _PathOther

type _PathState is (_PathSep | _PathDot | _PathDot2 | _PathOther)

primitive Path
  """
  Operations on paths that do not require a capability. The operations can be
  used to manipulate path names, but give no access to the resulting paths.
  """
  fun is_sep(c: U8): Bool =>
    """
    Determine if a byte is a path separator.
    """
    ifdef windows then
      (c == '/') or (c == '\\')
    else
      c == '/'
    end

  fun tag sep(): String =>
    """
    Return the path separator as a string.
    """
    ifdef windows then "\\" else "/" end

  fun is_abs(path: String): Bool =>
    """
    Return true if the path is an absolute path.
    """
    try
      ifdef windows then
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
        if is_sep(path(path.size()-1)) then
          if is_sep(next_path(0)) then
            return clean(path + next_path.trim(1))
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
    Convert / to the OS separator.
    Remove instances of . from the path.
    Remove instances of .. and the preceding path element from the path.
    The result will have no trailing slash unless it is a root directory.
    If the result would be empty, "." will be returned instead.
    """
    let s = recover String(path.size()) end
    let vol = volume(path)
    s.append(vol)

    var state: _PathState = _PathOther
    var i = vol.size()
    var backtrack = ISize(-1)
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
        backtrack = s.size().isize()
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
                backtrack = vol.size().isize()
              end

              if
                (s.size() == 0) or
                (s.compare_sub("../", 3, backtrack) is Equal) or
                ifdef windows then
                  s.compare_sub("..\\", 3, backtrack) is Equal
                else
                  true
                end
              then
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
            backtrack = s.size().isize()
            s.append("...")
            state = _PathOther
          | _PathOther =>
            s.append(".")
          end
        else
          match state
          | _PathSep =>
            backtrack = s.size().isize()
          | _PathDot =>
            backtrack = s.size().isize()
            s.append(".")
          | _PathDot2 =>
            backtrack = s.size().isize()
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
      if is_sep(s(s.size()-1)) then
        s.delete(-1, sep().size())
      end
    end

    if s.size() > 0 then
      s
    else
      "."
    end

  fun normcase(path: String): String =>
    """
    Normalizes the case of path for the runtime platform.
    """
    if Platform.windows() then
      recover val path.lower().replace("/", "\\") end
    elseif Platform.osx() then
      path.lower()
    else
      path
    end

  fun cwd(): String =>
    """
    Returns the program's working directory. Setting the working directory is
    not supported, as it is not concurrency-safe.
    """
    recover String.from_cstring(@pony_os_cwd[Pointer[U8]]()) end

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

    var to_i: ISize = 0

    ifdef windows then
      to_clean = abs(to_clean)
      target_clean = abs(target_clean)

      let to_vol = volume(to_clean)
      let target_vol = volume(target_clean)

      if to_vol != target_vol then
        error
      end

      to_i = to_vol.size().isize()
    end

    var to_0 = to_i
    var target_i = to_i
    var target_0 = target_i

    while true do
      to_i = try
        to_clean.find(sep(), to_i)
      else
        to_clean.size().isize()
      end

      target_i = try
        target_clean.find(sep(), target_i)
      else
        target_clean.size().isize()
      end

      if
        (to_i != target_i) or
        (to_clean.compare_sub(target_clean, target_i.usize()) isnt Equal)
      then
        break
      end

      if to_i < to_clean.size().isize() then
        to_i = to_i + 1
      end

      if target_i < target_clean.size().isize() then
        target_i = target_i + 1
      end

      to_0 = to_i
      target_0 = target_i
    end

    if
      ((to_i - to_0) == 2) and
      (to_clean.compare_sub("..", 2, to_0) is Equal)
    then
      error
    end

    if to_0.usize() != to_clean.size() then
      let result = recover String end

      try
        while true do
          to_i = to_clean.find(sep(), to_i) + 1
          result.append("..")
          result.append(sep())
        end
      end

      result.append("..")
      result.append(sep())
      result.append(target_clean.trim(target_0.usize()))
      result
    else
      target_clean.trim(target_0.usize())
    end

  fun split(path: String, separator: String = Path.sep()): (String, String) =>
    """
    Splits the path into a pair, (head, tail) where tail is the last pathname
    component and head is everything leading up to that. The tail part will
    never contain a slash; if path ends in a slash, tail will be empty. If
    there is no slash in path, head will be empty. If path is empty, both head
    and tail are empty. The path in head will be cleaned before it is returned.
    In all cases, join(head, tail) returns a path to the same location as path
    (but the strings may differ). Also see the functions dir() and base().
    """
    try
      let i = path.rfind(separator).usize()
      (clean(path.trim(0, i)), path.trim(i+separator.size()))
    else
      ("", path)
    end

  fun base(path: String): String =>
    """
    Return the path after the last separator, or the whole path if there is no
    separator.
    """
    try
      path.trim(path.rfind(sep()).usize() + 1)
    else
      path
    end

  fun dir(path: String): String =>
    """
    Return a cleaned path before the last separator, or the whole path if there
    is no separator.
    """
    try
      clean(path.trim(0, path.rfind(sep()).usize()))
    else
      path
    end

  fun ext(path: String): String =>
    """
    Return the file extension, i.e. the part after the last dot as long as that
    dot is after all separators. Return an empty string for no extension.
    """
    try
      let i = path.rfind(".")

      let j = try
        path.rfind(sep())
      else
        i
      end

      if i >= j then
        return path.trim(i.usize() + 1)
      end
    end
    ""

  fun volume(path: String): String =>
    """
    On Windows, this returns the drive letter or UNC base at the beginning of
    the path, if there is one. Otherwise, this returns an empty string.
    """
    ifdef windows then
      var offset = ISize(0)

      if path.compare_sub("""\\?\""", 4) is Equal then
        offset = 4

        if path.compare_sub("""UNC\""", 4, offset) is Equal then
          return _network_share(path, offset + 4)
        end
      end

      if _drive_letter(path, offset) then
        return path.trim(0, offset.usize() + 2)
      end

      try
        if
          is_sep(path.at_offset(offset)) and
          is_sep(path.at_offset(offset + 1))
        then
          return _network_share(path, offset + 3)
        end
      end
    end
    ""

  fun _drive_letter(path: String, offset: ISize = 0): Bool =>
    """
    Look for a drive letter followed by a ':', returning true if we find it.
    """
    try
      let c = path.at_offset(offset)

      (((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z'))) and
        (path.at_offset(offset + 1) == ':')
    else
      false
    end

  fun _network_share(path: String, offset: ISize = 0): String =>
    """
    Look for a host, a \, and a resource. Return the path up to that point if
    we found one, otherwise an empty String.
    """
    try
      let next = path.find("\\", offset) + 1

      try
        path.trim(0, path.find("\\", next).usize())
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
    ifdef windows then
      let s = path.clone()
      let len = s.size()
      var i = USize(0)

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
    ifdef windows then
      let s = path.clone()
      let len = s.size()
      var i = USize(0)

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

  fun canonical(path: String): String ? =>
    """
    Return the equivalent canonical absolute path. Raise an error if there
    isn't one.
    """
    let cstring = @pony_os_realpath[Pointer[U8] iso^](
      path.null_terminated().cstring())

    if cstring.is_null() then
      error
    else
      recover String.from_cstring(consume cstring) end
    end

  fun is_list_sep(c: U8): Bool =>
    """
    Determine if a byte is a path list separator.
    """
    ifdef windows then c == ';' else c == ':' end

  fun list_sep(): String =>
    """
    Return the path list separator as a string.
    """
    ifdef windows then ";" else ":" end

  fun split_list(path: String): Array[String] iso^ =>
    """
    Separate a list of paths into an array of cleaned paths.
    """
    let array = recover Array[String] end
    var offset: ISize = 0

    try
      while true do
        let next = path.find(list_sep(), offset)
        array.push(clean(path.trim(offset.usize(), next.usize())))
        offset = next + 1
      end
    else
      array.push(clean(path.trim(offset.usize())))
    end

    array

  fun random(len: USize = 6): String =>
    """
    Returns a pseudo-random base, suitable as a temporary file name or
    directory name, but not guaranteed to not already exist.
    """
    let letters =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    let s = recover String(len) end
    var n = USize(0)
    var r = Time.nanos().usize()

    try
      while n < len do
        let c = letters(r % letters.size())
        r = r / letters.size()
        s.push(c)
        n = n + 1
      end
    end
    s
