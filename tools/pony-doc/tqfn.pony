primitive TQFN
  """
  Constructs a Type Qualified Full Name (TQFN) from a package's
  qualified name and a type name.

  Format: `{qualified_package_name}-{type_name}` with all `/`
  replaced by `-`. Matches `write_tqfn()` in docgen.c (lines 226-257).

  The `type_name` parameter can be overridden to produce special
  TQFNs, e.g. passing `"-index"` for package index pages produces
  a double dash: `{pkg}--index`.
  """
  fun apply(qualified_name: String, type_name: String): String =>
    let raw: String val = qualified_name + "-" + type_name
    let result = recover iso String(raw.size()) end
    for byte in raw.values() do
      ifdef windows then
        if (byte == '/') or (byte == '\\') then
          result.push('-')
        else
          result.push(byte)
        end
      else
        if byte == '/' then
          result.push('-')
        else
          result.push(byte)
        end
      end
    end
    consume result
