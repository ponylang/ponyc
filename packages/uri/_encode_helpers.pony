primitive _FormFieldEncode
  """
  Encode a form-urlencoded field (key or value).

  Applies standard query encoding via `PercentEncode(input, URIPartQuery)`,
  then additionally encodes the structural delimiters `=`, `&`, and `+` which
  `PercentEncode` leaves alone (they are sub-delimiters allowed in query).
  """
  fun apply(input: String val): String val =>
    let base = PercentEncode(input, URIPartQuery)
    let out = String(base.size())
    for c in base.values() do
      if c == '=' then
        out.append("%3D")
      elseif c == '&' then
        out.append("%26")
      elseif c == '+' then
        out.append("%2B")
      else
        out.push(c)
      end
    end
    out.clone()

primitive _PathSegmentEncode
  """
  Encode a single path segment.

  Applies standard path encoding via `PercentEncode(input, URIPartPath)`,
  then additionally encodes `/` which `PercentEncode` leaves alone (it is
  a valid path character but acts as the segment separator).
  """
  fun apply(input: String val): String val =>
    let base = PercentEncode(input, URIPartPath)
    let out = String(base.size())
    for c in base.values() do
      if c == '/' then
        out.append("%2F")
      else
        out.push(c)
      end
    end
    out.clone()
