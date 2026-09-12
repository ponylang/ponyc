primitive QueryParams
  """
  Encode key-value pairs as a URL query string using RFC 3986 percent-encoding.

  Returns a string like `key1=value1&key2=value2` with all keys and values
  percent-encoded. No leading `?` — the caller or request builder prepends
  that when appending to a path.

  Returns an empty string for empty params.
  """

  fun apply(params: Array[(String, String)] val): String =>
    """
    Encode `params` as a query string.

    Keys and values are percent-encoded per RFC 3986. Pairs are joined with
    `&`. Returns an empty string if `params` is empty.
    """
    if params.size() == 0 then return "" end

    var buf = recover iso String end
    var first = true
    for (key, value) in params.values() do
      if not first then buf.push('&') end
      first = false
      buf.append(_PercentEncoder.query(key))
      buf.push('=')
      buf.append(_PercentEncoder.query(value))
    end
    consume buf
