primitive PathSanitize
  """
  Sanitizes module names for use in source directory paths.

  Replaces `.` with `_` and `/` with `-`.
  """
  fun replace_path_separator(name: String): String =>
    let result = recover iso String(name.size()) end
    for byte in name.values() do
      if byte == '.' then
        result.push('_')
      elseif byte == '/' then
        result.push('-')
      else
        result.push(byte)
      end
    end
    consume result
