primitive PathValidator
  """
  Validates archive entry paths. Rejects paths that could escape the target
  directory, contain backslashes or NUL bytes, empty or dot segments, or
  Windows drive letters.
  """
  fun apply(path: String): Bool =>
    """
    Returns `true` when `path` is safe to extract inside an archive target
    directory.
    """
    if path.size() == 0 then
      return false
    end

    try
      if path(0)? == '/' then
        return false
      end

      if (path.size() >= 2) and (path(1)? == ':') then
        let first = path(0)?
        if ((first >= 'A') and (first <= 'Z')) or
          ((first >= 'a') and (first <= 'z'))
        then
          return false
        end
      end

      var i: USize = 0
      while i < path.size() do
        let c = path(i)?
        if c == '\\' then
          return false
        end
        if c == 0 then
          return false
        end
        i = i + 1
      end
    else
      _Unreachable()
      return false
    end

    let segments = path.split_by("/")
    for segment in (consume segments).values() do
      if segment == ".." then
        return false
      end
      if segment == "." then
        return false
      end
      if segment.size() == 0 then
        return false
      end
    end

    true
