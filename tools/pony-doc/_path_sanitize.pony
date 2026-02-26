primitive PathSanitize
  """
  Sanitizes package names for use in source directory paths.

  Replaces `.` with `_` and `/` with `-`. On Windows, also replaces
  `\\` with `-`. Matches docgen.c `replace_path_separator()` at
  line 890. Separate from TQFN which only replaces `/` with `-`.
  """
  fun replace_path_separator(name: String): String =>
    let result = recover iso String(name.size()) end
    for byte in name.values() do
      if byte == '.' then
        result.push('_')
      elseif byte == '/' then
        result.push('-')
      else
        ifdef windows then
          if byte == '\\' then
            result.push('-')
          else
            result.push(byte)
          end
        else
          result.push(byte)
        end
      end
    end
    consume result
