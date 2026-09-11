primitive FormEncoder
  """
  Encode key-value pairs as `application/x-www-form-urlencoded` body data.

  Uses WHATWG form encoding: spaces become `+`, unreserved characters
  (`A-Z a-z 0-9 * - . _`) pass through, everything else is `%XX`.

  Returns `Array[U8] val` because the output is used as a request body,
  matching `HTTPRequest.body`'s type. Use `QueryParams` instead when building
  URL query strings (which use RFC 3986 encoding and return `String`).
  """

  fun apply(params: Array[(String, String)] val): Array[U8] val =>
    """
    Encode `params` as form-urlencoded body data.

    Keys and values are encoded per WHATWG spec. Pairs are joined with `&`.
    Returns an empty array if `params` is empty.
    """
    if params.size() == 0 then return recover val Array[U8] end end

    var buf = recover iso String end
    var first = true
    for (key, value) in params.values() do
      if not first then buf.push('&') end
      first = false
      buf.append(_PercentEncoder.form(key))
      buf.push('=')
      buf.append(_PercentEncoder.form(value))
    end
    (consume buf).iso_array()
