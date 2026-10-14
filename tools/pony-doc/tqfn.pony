primitive TQFN
  """
  Constructs a Type Qualified Full Name (TQFN) from a module's
  qualified name and a type name.

  Format: `{qualified_module_name}-{type_name}` with `/` replaced by `-`.

  The `type_name` parameter can be overridden to produce special
  TQFNs, e.g. passing `"-index"` for module index pages produces
  a double dash: `{mod}--index`.
  """
  fun apply(
    qualified_name: String,
    type_name: String)
    : String
  =>
    """
    Build the TQFN string from a qualified module name
    and a type name.
    """
    let raw: String val = qualified_name + "-" + type_name
    let result = recover iso String(raw.size()) end
    for byte in raw.values() do
      if byte == '/' then
        result.push('-')
      else
        result.push(byte)
      end
    end
    consume result
