primitive PercentEncoding
  """
  Percent-encoding for the path component of a file URI, as defined by
  RFC 3986.

  Both functions operate on bytes, so a multi-byte UTF-8 sequence encodes to
  one escape per byte.
  """

  fun encode(s: String): String =>
    """
    Percent-encode every byte that may not appear literally in a URI path.

    Hex digits are uppercase, which RFC 3986 section 2.1 specifies for URI
    producers. The `/` separator is left alone, so a whole path can be passed
    in one call.
    """
    var clean = true
    for b in s.values() do
      if not _literal(b) then
        clean = false
        break
      end
    end
    if clean then
      return s
    end

    let out = recover String(s.size()) end
    for b in s.values() do
      if _literal(b) then
        out.push(b)
      else
        out.push('%')
        out.push(_hex_digit(b >> 4))
        out.push(_hex_digit(b and 0xF))
      end
    end
    consume out

  fun decode(s: String): String =>
    """
    Replace every `%` followed by two hex digits with the byte it denotes.
    Hex digits of either case are accepted.

    A `%` that is not followed by two hex digits is kept as it is. Clients
    decode that way too, and a language server has nowhere to report a
    malformed URI: the only thing left is to try the path, which fails if no
    such file exists.
    """
    if not s.contains("%") then
      return s
    end

    let out = recover String(s.size()) end
    var i = USize(0)
    try
      while i < s.size() do
        let b = s(i)?
        if b != '%' then
          out.push(b)
          i = i + 1
        else
          match
            if (i + 2) < s.size() then
              (_hex_value(s(i + 1)?), _hex_value(s(i + 2)?))
            else
              (None, None)
            end
          | (let hi: U8, let lo: U8) =>
            out.push((hi << 4) or lo)
            i = i + 3
          else
            out.push('%')
            i = i + 1
          end
        end
      end
    else
      // Every index read above is guarded by a size check.
      _Unreachable()
    end
    consume out

  fun _literal(b: U8): Bool =>
    """
    True if `b` may appear literally in a URI path: RFC 3986 `pchar`, plus the
    `/` that separates segments.
    """
    if
      ((b >= 'A') and (b <= 'Z')) or
      ((b >= 'a') and (b <= 'z')) or
      ((b >= '0') and (b <= '9'))
    then
      true
    else
      match b
      // unreserved
      | '-' | '.' | '_' | '~'
      // sub-delims
      | '!' | '$' | '&' | '\'' | '(' | ')' | '*' | '+' | ',' | ';' | '='
      // the rest of pchar
      | ':' | '@'
      // segment separator
      | '/' => true
      else
        false
      end
    end

  fun _hex_digit(v: U8): U8 =>
    if v < 10 then '0' + v else 'A' + (v - 10) end

  fun _hex_value(b: U8): (U8 | None) =>
    if (b >= '0') and (b <= '9') then
      b - '0'
    elseif (b >= 'A') and (b <= 'F') then
      (b - 'A') + 10
    elseif (b >= 'a') and (b <= 'f') then
      (b - 'a') + 10
    else
      None
    end
