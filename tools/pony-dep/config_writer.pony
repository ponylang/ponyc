primitive ConfigWriter
  """
  Formats dependency entries as `pony.deps` configuration file text.
  """
  fun format_entry(
    name: String val,
    dep_type: String val,
    url: String val,
    hash: String val,
    ref_name: (String val | None) = None,
    documentation_url: (String val | None) = None)
    : String val
  =>
    """
    Returns a formatted `dep`/`end` block.
    """
    recover val
      let s = String
      s .> append("dep ") .> append(name)
        .> append("\n  type ") .> append(dep_type)
        .> append("\n  url ") .> append(url)
      match ref_name
      | let r: String val =>
        s .> append("\n  ref ") .> append(r)
      end
      s .> append("\n  hash ") .> append(hash)
      match documentation_url
      | let d: String val =>
        s .> append("\n  documentation_url ") .> append(d)
      end
      s .> append("\nend\n")
      s
    end

  fun new_config_with(entry: String val): String val =>
    """
    Returns a complete config file containing a `version 1` header and one
    dep entry.
    """
    "version 1\n\n" + entry
