primitive DepNameValidator
  """
  Validates dependency names for use in `pony.deps` configuration files.
  A valid dep name is a non-empty single token that does not contain `/`,
  `\`, or NUL, and is not `.` or `..`.
  """
  fun apply(name: String val): (None | String val) =>
    """
    Returns `None` when `name` is valid, or an error description.
    """
    if name.size() == 0 then
      return "'dep' requires a name"
    end

    try
      name.find(" ")?
      return "dep name must be a single token"
    end
    try
      name.find("\t")?
      return "dep name must be a single token"
    end

    if (name == ".") or (name == "..") then
      return "dep name must not be '.' or '..'"
    end

    var i: USize = 0
    while i < name.size() do
      let c =
        try name(i)?
        else _Unreachable(); return "dep name contains invalid character"
        end
      if (c == '/') or (c == '\\') or (c == 0) then
        return "dep name contains invalid character"
      end
      i = i + 1
    end
    None
