use "collections"

// A variable value: a string, a list of strings, or an associative array
// of key-value string pairs.
type URITemplateValue is
  ( String val
  | Array[String val] val
  | Array[(String val, String val)] val )

class URITemplateVariables
  """
  A collection of named variable bindings for URI template expansion.

  Variables can be strings, lists (ordered sequences of strings), or
  associative arrays (ordered sequences of key-value string pairs).
  Variables not present in the collection are treated as undefined
  per RFC 6570 and produce no output during expansion.
  """
  let _vars: Map[String val, URITemplateValue]

  new create() =>
    _vars = Map[String val, URITemplateValue]

  fun ref set(name: String, value: String) =>
    """
    Bind a string variable.
    """
    _vars(name) = value

  fun ref set_list(name: String, values: Array[String val] val) =>
    """
    Bind a list variable.
    """
    _vars(name) = values

  fun ref set_pairs(
    name: String,
    pairs: Array[(String val, String val)] val)
  =>
    """
    Bind an associative array variable.
    """
    _vars(name) = pairs

  fun box _get(name: String): (URITemplateValue | None) =>
    try
      _vars(name)?
    else
      None
    end
