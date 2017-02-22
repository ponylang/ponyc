primitive URLPartUser
primitive URLPartPassword
primitive URLPartHost
primitive URLPartPath
primitive URLPartQuery
primitive URLPartFragment

type URLPart is (URLPartUser | URLPartPassword | URLPartHost |
  URLPartPath | URLPartQuery | URLPartFragment)


primitive URLEncode
  """
  Functions for checking, encoding, and decoding parts of URLs.
  """

  fun encode(from: String, part: URLPart, percent_encoded: Bool = true)
    : String ?
  =>
    """
    URL encode and normilase the given string.
    The percent_encoded parameter indicates how '%' characters should be
    interpretted.
    true => given string is already at least partially encoded, so '%'s
      indicate an encoded character.
    false => given string is not yet encoded at all, so '%'s are just '%'s.
    An error is raised on invalid existing encoding or illegal characters that
    cannot be encoded.
    """
    if _is_host_ipv6(from, part) then
      return from
    end

    let out = recover String(from.size()) end
    var i = USize(0)

    while i < from.size() do
      var c = from(i)
      var should_encode = false

      if (c == '%') and percent_encoded then
        // Treat % as an encoded character.
        // _unhex() will throw on bad / missing hex digit.
        c = (_unhex(from(i + 1)) << 4) or _unhex(from(i + 2))
        should_encode = not _normal_decode(c, part)
        i = i + 3
      else
        // Not an encoded character.
        should_encode = not _is_char_legal(c, part)
        i = i + 1
      end

      if should_encode then
        out.push('%')
        out.push(_hex(c >> 4))
        out.push(_hex(c and 0xF))
      else
        out.push(c)
      end
    end

    out

  fun decode(from: String): String ? =>
    """
    URL decode a string. Raise an error on invalid URL encoded.
    """
    let out = recover String(from.size()) end
    var i = USize(0)

    while i < from.size() do
      let c = from(i)

      if c == '%' then
        // _unhex() will throw on bad / missing hex digit.
        let value = (_unhex(from(i + 1)) << 4) or _unhex(from(i + 2))
        out.push(value)
        i = i + 3
      elseif c == '+' then
        out.push(' ')
        i = i + 1
      else
        out.push(c)
        i = i + 1
      end
    end

    out

  fun check_scheme(scheme: String): Bool =>
    """
    Check that the given string is a valid scheme.
    """
    try
      var i = USize(0)

      while i < scheme.size() do
        let c = scheme(i)
        
        if ((c < 'a') or (c > 'z')) and
          ((c < 'A') or (c > 'Z')) and
          ((c < '0') or (c > '9')) and
          (c != '-') and (c != '+') and (c != '.') then
          return false
        end
        
        i = i + 1
      end
    end

    true

  fun check(from: String, part: URLPart): Bool =>
    """
    Check that the given string is valid to be the given URL part without
    further encoding.
    Canonical form is not checked for, merely validity.
    """
    try
      if _is_host_ipv6(from, part) then
        return true
      end
    else
      return false
    end

    try
      var i = USize(0)

      while i < from.size() do
        let c = from(i)
        
        if c == '%' then
          // Character is encoded.
          // _unhex() will throw on bad / missing hex digit.
          _unhex(from(i + 1))
          _unhex(from(i + 2))
          i = i + 3
        elseif _is_char_legal(c, part) then
          i = i + 1
        else
          return false
        end
      end
      true
    else
      false
    end

  fun _is_char_legal(value: U8, part: URLPart): Bool =>
    """
    Determine whether the given character is legal to appear in the specified
    URL part.
    """
    // The unreserved and sub-delim characters are always allowed.
    if ((value >= 'a') and (value <= 'z')) or
      ((value >= 'A') and (value <= 'Z')) or
      ((value >= '0') and (value <= '9')) or
      (value == '-') or (value == '.') or (value == '_') or (value == '~') or
      (value == '!') or (value == '$') or (value == '&') or (value == '\'') or
      (value == '(') or (value == ')') or (value == '*') or (value == '+') or
      (value == ',') or (value == ';') or (value == '=') then
      return true
    end

    // Which general delims are allowed depends on the part.
    match part
    | URLPartPassword => (value == ':')
    | URLPartPath => (value == ':') or (value == '@') or (value == '/')
    | URLPartQuery =>
      (value == ':') or (value == '@') or (value == '/') or (value == '?')
    | URLPartFragment =>
      (value == ':') or (value == '@') or (value == '/') or (value == '?')
    else
      false
    end
    
  fun _normal_decode(value: U8, part: URLPart): Bool =>
    """
    Determine whether the given character should be decoded to give normal
    form.
    Some characters, such as sub-delims, are valid to have either in encoded or
    unencoded form. These should be left as they are when normalising. This
    will return false for such characters.
    """
    // The unreserved characters should always be decoded.
    if ((value >= 'a') and (value <= 'z')) or
      ((value >= 'A') and (value <= 'Z')) or
      ((value >= '0') and (value <= '9')) or
      (value == '-') or (value == '_') or (value == '.') or (value == '~') then
      return true
    end

    // Which general delims to decode depends on the part.
    match part
    | URLPartPassword => (value == ':')
    | URLPartPath => (value == ':') or (value == '@') or (value == '/')
    | URLPartQuery =>
      (value == ':') or (value == '@') or (value == '/') or (value == '?')
    | URLPartFragment =>
      (value == ':') or (value == '@') or (value == '/') or (value == '?')
    else
      false
    end

  fun _is_host_ipv6(host: String, part: URLPart): Bool ? =>
    """
    Check whether the given string is a valid IPv6 format host.
    Returns:
      true if string is a valid IPv6 format host.
      false if string is not an IPv6 foramt host at all.
      Raises an error if string is an invalid IPv6 format host.
    """
    try
      if (part isnt URLPartHost) or (host.size() == 0) or (host(0) != '[') then
        return false
      end
    end

    // We are an IPv6 format host, ie a host starting with a '['.
    var i = USize(1)

    while i < (host.size() - 1) do
      let c = host(i)

      // Only hex digits, ':' and '.' and allowed.
      if ((c < 'a') or (c > 'f')) and
        ((c < 'A') or (c > 'F')) and
        ((c < '0') or (c > '9')) and
        (c != ':') and (c != '.') then
        error
      end
        
      i = i + 1
    end

    // Must end with a ']'.
    if host(host.size() - 1) != ']' then error end
    true

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
