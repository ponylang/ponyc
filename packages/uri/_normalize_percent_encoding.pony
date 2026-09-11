primitive _NormalizePercentEncoding
  """
  Normalize percent-encoded sequences in a URI component string.

  For each `%XX` sequence: if the decoded byte is an unreserved character
  (RFC 3986 section 2.3), emit the literal character; otherwise emit the
  `%XX` triplet with uppercase hex digits. Non-percent characters pass
  through unchanged.

  Returns `InvalidPercentEncoding` for truncated sequences (`%` or `%A`
  at end of string) or non-hex digits after `%`.
  """
  fun apply(input: String val): (String val | InvalidPercentEncoding val) =>
    // Fast path: no percent signs means nothing to normalize
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
          let hi = PercentDecode._hex_value(input(i + 1)?)?
          let lo = PercentDecode._hex_value(input(i + 2)?)?
          let byte = (hi * 16) + lo
          if PercentEncode._is_unreserved(byte) then
            out.push(byte)
          else
            out.append(PercentEncode._encode_byte(byte))
          end
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
