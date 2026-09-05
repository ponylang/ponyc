primitive PathValidator
  """
  Validates archive entry paths. Rejects paths that could escape the target
  directory, contain backslashes or NUL bytes, or have empty or dot segments.
  """
  fun apply(path: String): Bool =>
    if path.size() == 0 then
      return false
    end

    try
      if path(0)? == '/' then
        return false
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
