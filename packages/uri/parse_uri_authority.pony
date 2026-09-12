primitive ParseURIAuthority
  """
  Parse a standalone authority string (`[userinfo@]host[:port]`).

  Use this for HTTP CONNECT request-targets (authority-form) where the
  request-target is `host:port` without a scheme or `//` prefix.
  `ParseURI` would misinterpret `host:port` as `scheme:path`.

  The `@` delimiter is identified by scanning for the last `@` in the
  authority string, following the WHATWG URL Standard convention. RFC 3986
  grammar makes the first `@` the delimiter (userinfo cannot contain
  literal `@`, only `%40`), but using the last `@` is more robust against
  real-world URIs with unencoded `@` in userinfo.
  """
  fun apply(raw: String val): (URIAuthority val | URIParseError val) =>
    _parse(raw, 0, raw.size())

  fun _parse(raw: String val, start: USize, end_pos: USize)
    : (URIAuthority val | URIParseError val)
  =>
    // Find @ for userinfo
    var userinfo: (String | None) = None
    var host_start = start

    // Check for IP-literal first — if the host starts with '[', @ before
    // the closing ']' is part of the IP-literal, not a userinfo delimiter.
    var at_pos: USize = end_pos // sentinel: no @ found
    try
      if (host_start < end_pos) and (raw(host_start)? == '[') then
        // IP-literal: find closing ']' first, then look for @ before '['
        // Actually, in authority "[::1]:8080", there's no userinfo.
        // But in "user@[::1]:8080", the @ is before the '['.
        // We need to find the LAST @ before any '[' or just the last @.
        // RFC 3986: userinfo = *( unreserved / pct-encoded / sub-delims / ":" )
        // The @ delimiter separates userinfo from host.
        // Scan for @ only before the '['.
        None // no @ possible when first char is '['
      else
        // Scan for last @ (RFC 3986 says first @ delimits, but in practice
        // the last @ is used since userinfo can contain encoded @)
        var i = start
        while i < end_pos do
          if raw(i)? == '@' then
            at_pos = i
          end
          i = i + 1
        end
      end
    else
      _Unreachable()
    end

    if at_pos < end_pos then
      let ui: String val = raw.substring(start.isize(), at_pos.isize())
      userinfo = ui
      host_start = at_pos + 1
    end

    // Check for IP-literal (starts with '[')
    try
      if (host_start < end_pos) and (raw(host_start)? == '[') then
        // Find closing ']'
        var bracket_end: USize = host_start + 1
        var found = false
        while bracket_end < end_pos do
          if raw(bracket_end)? == ']' then
            found = true
            break
          end
          bracket_end = bracket_end + 1
        end
        if not found then
          return InvalidHost
        end

        // Validate IP-literal content
        let literal_content: String val =
          raw.substring((host_start + 1).isize(), bracket_end.isize())
        if not _valid_ip_literal(literal_content) then
          return InvalidHost
        end

        let host_str: String val =
          raw.substring(host_start.isize(), (bracket_end + 1).isize())

        // After ']', optional ':port'
        let after_bracket = bracket_end + 1
        if after_bracket < end_pos then
          try
            if raw(after_bracket)? == ':' then
              let port_str: String val =
                raw.substring((after_bracket + 1).isize(), end_pos.isize())
              match _parse_port(port_str)
              | let p: U16 =>
                return URIAuthority(userinfo, host_str, p)
              | None =>
                return URIAuthority(userinfo, host_str, None)
              | let e: URIParseError val =>
                return e
              end
            else
              return InvalidHost
            end
          else
            _Unreachable()
          end
        end
        return URIAuthority(userinfo, host_str, None)
      end
    else
      _Unreachable()
    end

    // Regular host (reg-name or IPv4) — find ':' for port, scanning from end
    // to handle IPv6 addresses (though those should be in brackets)
    var colon_pos: USize = end_pos // sentinel
    try
      var i = end_pos
      while i > host_start do
        i = i - 1
        if raw(i)? == ':' then
          colon_pos = i
          break
        end
      end
    else
      _Unreachable()
    end

    if colon_pos < end_pos then
      let host_str: String val =
        raw.substring(host_start.isize(), colon_pos.isize())
      let port_str: String val =
        raw.substring((colon_pos + 1).isize(), end_pos.isize())
      match \exhaustive\ _parse_port(port_str)
      | let p: U16 =>
        URIAuthority(userinfo, host_str, p)
      | None =>
        URIAuthority(userinfo, host_str, None)
      | let e: URIParseError val =>
        e
      end
    else
      let host_val: String val =
        raw.substring(host_start.isize(), end_pos.isize())
      URIAuthority(userinfo, host_val, None)
    end

  fun _parse_port(port_str: String val): (U16 | None | URIParseError val) =>
    if port_str.size() == 0 then
      // Empty port (e.g., "host:") — RFC 3986 allows empty port
      return None
    end

    // All characters must be digits
    for c in port_str.values() do
      if (c < '0') or (c > '9') then
        return InvalidPort
      end
    end

    // Parse as U64 first to detect overflow
    try
      let value = port_str.u64()?
      if value > 65535 then
        return InvalidPort
      end
      value.u16()
    else
      InvalidPort
    end

  fun _valid_ip_literal(content: String val): Bool =>
    if content.size() == 0 then
      return false
    end

    // IPvFuture: starts with 'v' (case-insensitive)
    try
      let first = content(0)?
      if (first == 'v') or (first == 'V') then
        return _valid_ipvfuture(content)
      end
    else
      _Unreachable() // content.size() > 0 guaranteed above
    end

    // Otherwise must be IPv6
    _valid_ipv6(content)

  fun _valid_ipv6(content: String val): Bool =>
    // Basic validation: only hex digits, ':', and '.' (for IPv4-mapped)
    for c in content.values() do
      if not (
        ((c >= '0') and (c <= '9')) or
          ((c >= 'A') and (c <= 'F')) or
          ((c >= 'a') and (c <= 'f')) or
          (c == ':') or (c == '.'))
      then
        return false
      end
    end
    true

  fun _valid_ipvfuture(content: String val): Bool =>
    // IPvFuture = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
    // Must have at least "v" + hexdig + "." + char = 4 chars
    if content.size() < 4 then
      return false
    end

    // Find the '.' separator
    var dot_pos: USize = 0
    var found_dot = false
    try
      var i: USize = 1  // skip 'v'
      while i < content.size() do
        if content(i)? == '.' then
          dot_pos = i
          found_dot = true
          break
        end
        i = i + 1
      end
    else
      _Unreachable()
    end

    if not found_dot then
      return false
    end

    // Must have at least one hex digit between 'v' and '.'
    if dot_pos < 2 then
      return false
    end

    // Validate hex digits between 'v' and '.'
    try
      var i: USize = 1
      while i < dot_pos do
        let c = content(i)?
        if not (
          ((c >= '0') and (c <= '9')) or
            ((c >= 'A') and (c <= 'F')) or
            ((c >= 'a') and (c <= 'f')))
        then
          return false
        end
        i = i + 1
      end
    else
      _Unreachable()
    end

    // Must have at least one char after '.'
    if (dot_pos + 1) >= content.size() then
      return false
    end

    // Validate chars after '.': unreserved / sub-delims / ":"
    try
      var i: USize = dot_pos + 1
      while i < content.size() do
        let c = content(i)?
        if not (_is_unreserved(c) or _is_sub_delim(c) or (c == ':')) then
          return false
        end
        i = i + 1
      end
    else
      _Unreachable()
    end

    true

  fun _is_unreserved(c: U8): Bool =>
    ((c >= 'A') and (c <= 'Z')) or
      ((c >= 'a') and (c <= 'z')) or
      ((c >= '0') and (c <= '9')) or
      (c == '-') or (c == '.') or (c == '_') or (c == '~')

  fun _is_sub_delim(c: U8): Bool =>
    (c == '!') or (c == '$') or (c == '&') or (c == '\'') or
      (c == '(') or (c == ')') or (c == '*') or (c == '+') or
      (c == ',') or (c == ';') or (c == '=')
