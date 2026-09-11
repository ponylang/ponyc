primitive InvalidPercentEncoding is Stringable
  """
  Percent-encoded sequence is truncated or contains non-hex digits.
  """
  fun string(): String iso^ => "InvalidPercentEncoding".clone()

primitive URIPartUserinfo
  """
  Encoding rules for the userinfo component.
  """

primitive URIPartHost
  """
  Encoding rules for the host component (reg-name only).
  """

primitive URIPartPath
  """
  Encoding rules for the path component.
  """

primitive URIPartQuery
  """
  Encoding rules for the query component.
  """

primitive URIPartFragment
  """
  Encoding rules for the fragment component.
  """

type URIPart is
  ( URIPartUserinfo | URIPartHost | URIPartPath
  | URIPartQuery | URIPartFragment )

primitive PercentDecode
  """
  Decode all percent-encoded sequences in the input.

  Does NOT decode `+` as space — that convention belongs to
  `application/x-www-form-urlencoded` (handled by `ParseFormURLEncoded`),
  not RFC 3986 percent-encoding.
  """
  fun apply(input: String val): (String val | InvalidPercentEncoding val) =>
    """
    Decode percent-encoded sequences in `input`.
    """
    // Fast path: no percent signs means nothing to decode
    if not input.contains("%") then
      return input
    end

    let out = String(input.size())
    var i: USize = 0
    while i < input.size() do
      try
        let c = input(i)?
        if c == '%' then
          if (i + 2) >= input.size() then
            return InvalidPercentEncoding
          end
          let hi = _hex_value(input(i + 1)?)?
          let lo = _hex_value(input(i + 2)?)?
          out.push((hi * 16) + lo)
          i = i + 3
        else
          out.push(c)
          i = i + 1
        end
      else
        return InvalidPercentEncoding
      end
    end
    out.clone()

  fun _hex_value(c: U8): U8 ? =>
    if (c >= '0') and (c <= '9') then
      c - '0'
    elseif (c >= 'A') and (c <= 'F') then
      (c - 'A') + 10
    elseif (c >= 'a') and (c <= 'f') then
      (c - 'a') + 10
    else
      error
    end

primitive PercentEncode
  """
  Percent-encode characters that are not allowed unencoded in the
  specified URI component per RFC 3986.

  For `URIPartHost`, this applies to reg-name hosts only. IP-literals
  (IPv6 addresses and IPvFuture in brackets) have their own syntax and
  should not be percent-encoded — pass them through unchanged.
  """
  fun apply(input: String val, part: URIPart): String val =>
    let out = String(input.size())
    for c in input.values() do
      if _allowed(c, part) then
        out.push(c)
      else
        out.append(_encode_byte(c))
      end
    end
    out.clone()

  fun _encode_byte(c: U8): String val =>
    let hex = "0123456789ABCDEF"
    recover val
      let out = String(3)
      out.push('%')
      try
        out.push(hex(c.usize() >> 4)?)
        out.push(hex(c.usize() and 0x0F)?)
      else
        _Unreachable()
      end
      out
    end

  fun _allowed(c: U8, part: URIPart): Bool =>
    // unreserved characters are always allowed (RFC 3986 section 2.3)
    if _is_unreserved(c) then
      return true
    end

    // sub-delims are allowed in all components
    if _is_sub_delim(c) then
      return true
    end

    // component-specific allowed characters
    match \exhaustive\ part
    | URIPartUserinfo => c == ':'
    | URIPartHost => false
    | URIPartPath => (c == ':') or (c == '@') or (c == '/')
    | URIPartQuery =>
      (c == ':') or (c == '@') or (c == '/') or (c == '?')
    | URIPartFragment =>
      (c == ':') or (c == '@') or (c == '/') or (c == '?')
    end

  fun _is_unreserved(c: U8): Bool =>
    // ALPHA / DIGIT / "-" / "." / "_" / "~"
    ((c >= 'A') and (c <= 'Z')) or
      ((c >= 'a') and (c <= 'z')) or
      ((c >= '0') and (c <= '9')) or
      (c == '-') or (c == '.') or (c == '_') or (c == '~')

  fun _is_sub_delim(c: U8): Bool =>
    // "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
    (c == '!') or (c == '$') or (c == '&') or (c == '\'') or
      (c == '(') or (c == ')') or (c == '*') or (c == '+') or
      (c == ',') or (c == ';') or (c == '=')
