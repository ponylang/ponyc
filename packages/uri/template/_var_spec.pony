class val _VarSpec
  """
  A variable specifier within a template expression.

  Combines a variable name with an optional modifier (prefix or explode).
  """
  let name: String val
  let modifier: _Modifier

  new val create(name': String val, modifier': _Modifier = _ModNone) =>
    name = name'
    modifier = modifier'

primitive _ModNone
  """
  No modifier.
  """

class val _ModPrefix
  """
  Prefix modifier (:N) — take first N codepoints.
  """
  let max_length: USize

  new val create(max_length': USize) =>
    max_length = max_length'

primitive _ModExplode
  """
  Explode modifier (*) — expand composite values.
  """

// A variable modifier: none, prefix (:N), or explode (*).
type _Modifier is (_ModNone | _ModPrefix | _ModExplode)
