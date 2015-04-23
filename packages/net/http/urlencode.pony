primitive URLEncode
  """
  Functions for checking, encoding, and decoding URLs.
  """
  fun encode(from: String, path: Bool = true, query: Bool = false): String ? =>
    """
    URL encode a string. If parts of it are already URL encoded, they will be
    made canonical, if they aren't already. If existing URL encoding is
    invalid, raise an error.
    """
    let out = recover String(from.size()) end
    var i = U64(0)

    while i < from.size() do
      let c = from(i)

      if _unreserved(c, path, query) then
        out.append_byte(c)
        i = i + 1
      elseif c == '%' then
        let hi = from(i + 1)
        let lo = from(i + 2)
        let value = (_unhex(hi) << 8) or _unhex(lo)

        if _unreserved(value) then
          out.append_byte(value)
        else
          out.append_byte('%')
          out.append_byte(hi)
          out.append_byte(lo)
        end

        i = i + 3
      else
        out.append_byte('%')
        out.append_byte(_hex(c >> 4))
        out.append_byte(_hex(c and 0xF))
        i = i + 1
      end
    end

    out

  fun decode(from: String, path: Bool = true, query: Bool = false,
    canonical: Bool = false): String ?
  =>
    """
    URL decode a string. Raise an error if it isn't a valid URL encoded string.
    """
    let out = recover String(from.size()) end
    var i = U64(0)

    while i < from.size() do
      let c = from(i)

      if _unreserved(c, path, query) then
        out.append_byte(c)
        i = i + 1
      elseif c == '%' then
        let value = (_hex(from(i + 1)) << 8) or _hex(from(i + 2))

        if canonical and _unreserved(value) then
          error
        end

        out.append_byte(value)
        i = i + 3
      else
        error
      end
    end

    out

  fun check(from: String, path: Bool = true, query: Bool = false,
    canonical: Bool = false): Bool
  =>
    """
    Return true if the string is a valid URL encoded string, false otherwise.
    If path is true, it accepts unencoded '/' characters. If query is true, it
    accepts unencoded '&' and '=' characters. If canonical is true, it raises
    an error if anything is %-encoded that shouldn't be.
    """
    try
      var i = U64(0)

      while i < from.size() do
        let c = from(i)

        if _unreserved(c, path, query) then
          i = i + 1
        elseif c == '%' then
          let value = (_hex(from(i + 1)) << 8) or _hex(from(i + 2))

          if canonical and _unreserved(value) then
            return false
          end

          i = i + 3
        else
          return false
        end
      end
      true
    else
      false
    end

  fun _unreserved(value: U8, path: Bool = false, query: Bool = false): Bool =>
    """
    Return true if it is an unreserved character. This includes '/' if path is
    true, and includes '&' and '=' if query is true.
    """
    ((value >= 'A') and (value <= 'Z')) or
      ((value >= 'a') and (value <= 'z')) or
      ((value >= '0') and (value <= '9')) or
      (value == '-') or (value == '_') or (value == '.') or (value == '~') or
      (path and (value == '/')) or
      (query and ((value == '&') or (value == '=')))

  fun _hex(value: U8): U8 ? =>
    """
    Turn 4 bits into a hex value.
    """
    if value < 10 then
      value + '0'
    elseif value < 16 then
      (value + 'A') - 10
    else
      error
    end

  fun _unhex(value: U8): U8 ? =>
    """
    Turn a hex value into 4 bits.
    """
    if (value >= '0') and (value <= '9') then
      value - '0'
    elseif (value >= 'A') and (value <= 'F') then
      (value - 'A') + 10
    elseif (value >= 'a') and (value <= 'f') then
      (value - 'a') + 10
    else
      error
    end
