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
      // RFC 3986 reg-name cannot contain ':'. The last-colon scan above
      // leaves ':' in host_str when the input has multiple colons.
      if host_str.contains(":") then
        return InvalidHost
      end
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
    """
    RFC 3986 IPv6address grammar. The content is the text between brackets,
    with no brackets.
    """
    if content.size() == 0 then return false end

    // Find :: position (at most one allowed)
    var dc_pos: ISize = -1
    try
      var i: USize = 0
      while (i + 1) < content.size() do
        if (content(i)? == ':') and (content(i + 1)? == ':') then
          dc_pos = i.isize()
          break
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    let has_dc = dc_pos != ISize(-1)

    if has_dc then
      let after = dc_pos.usize() + 2
      try
        var i = after
        while (i + 1) < content.size() do
          if (content(i)? == ':') and (content(i + 1)? == ':') then
            return false
          end
          i = i + 1
        end
      else
        _Unreachable()
      end
    end

    if has_dc then
      let left: String val =
        content.substring(0, dc_pos)
      let right: String val =
        content.substring(dc_pos + 2)

      let left_groups = _count_h16_groups(left)
      if left_groups == USize.max_value() then return false end

      if right.size() == 0 then
        // Ends with :: — left side can be 0..7 groups
        return left_groups <= 7
      end

      (let right_h16, let has_ipv4) = _parse_right_side(right)
      if right_h16 == USize.max_value() then return false end

      let total_represented =
        left_groups + right_h16 + if has_ipv4 then USize(2) else USize(0) end
      // :: expands to fill 8 groups, so left + right < 8
      total_represented <= 7
    else
      // No :: — must be exactly 8 h16 groups, or 6 h16 + IPv4
      _parse_no_dc(content)
    end

  fun _count_h16_groups(s: String val): USize =>
    """
    Count colon-separated h16 groups. Returns USize.max_value() on invalid
    input. An empty string returns 0.
    """
    if s.size() == 0 then return 0 end
    var count: USize = 0
    var group_start: USize = 0
    try
      var i: USize = 0
      while i < s.size() do
        if s(i)? == ':' then
          if not _is_h16(s, group_start, i) then
            return USize.max_value()
          end
          count = count + 1
          group_start = i + 1
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    // Last group after final colon (or entire string if no colons)
    if not _is_h16(s, group_start, s.size()) then
      return USize.max_value()
    end
    count + 1

  fun _is_h16(s: String val, from: USize, to: USize): Bool =>
    """
    1*4HEXDIG
    """
    let len = to - from
    if (len < 1) or (len > 4) then return false end
    try
      var i = from
      while i < to do
        let c = s(i)?
        if not _is_hexdig(c) then return false end
        i = i + 1
      end
    else
      _Unreachable()
    end
    true

  fun _is_hexdig(c: U8): Bool =>
    ((c >= '0') and (c <= '9')) or
      ((c >= 'A') and (c <= 'F')) or
      ((c >= 'a') and (c <= 'f'))

  fun _parse_right_side(s: String val): (USize, Bool) =>
    """
    Parse the portion after :: into h16 count and whether it ends with IPv4.
    Returns (USize.max_value(), false) on invalid input.
    """
    if s.contains(".") then
      var last_colon = ISize(-1)
      try
        var i = s.size()
        while i > 0 do
          i = i - 1
          if s(i)? == ':' then
            last_colon = i.isize()
            break
          end
        end
      else
        _Unreachable()
      end

      if last_colon == ISize(-1) then
        // No colon — entire right side is the IPv4 address
        if _valid_ipv4(s) then
          return (0, true)
        else
          return (USize.max_value(), false)
        end
      end

      let h16_part: String val = s.substring(0, last_colon)
      let ipv4_part: String val =
        s.substring(last_colon + 1)

      if not _valid_ipv4(ipv4_part) then
        return (USize.max_value(), false)
      end

      let h16_count = _count_h16_groups(h16_part)
      if h16_count == USize.max_value() then
        return (USize.max_value(), false)
      end
      (h16_count, true)
    else
      let h16_count = _count_h16_groups(s)
      if h16_count == USize.max_value() then
        return (USize.max_value(), false)
      end
      (h16_count, false)
    end

  fun _parse_no_dc(content: String val): Bool =>
    """
    No :: present — must be 8 h16 groups or 6 h16 ':' IPv4address.
    """
    if content.contains(".") then
      // RFC 3986: 6( h16 ":" ) IPv4address
      var last_colon_before_dot = ISize(-1)
      try
        var i: USize = 0
        while i < content.size() do
          if content(i)? == ':' then
            last_colon_before_dot = i.isize()
          elseif content(i)? == '.' then
            break
          end
          i = i + 1
        end
      else
        _Unreachable()
      end

      if last_colon_before_dot == ISize(-1) then return false end

      let h16_part: String val =
        content.substring(0, last_colon_before_dot)
      let ipv4_part: String val =
        content.substring(last_colon_before_dot + 1)

      if not _valid_ipv4(ipv4_part) then return false end

      let h16_count = _count_h16_groups(h16_part)
      h16_count == 6
    else
      _count_h16_groups(content) == 8
    end

  fun _valid_ipv4(s: String val): Bool =>
    """
    RFC 3986 IPv4address =
    dec-octet "." dec-octet "." dec-octet "." dec-octet
    """
    var octet_count: USize = 0
    var octet_start: USize = 0
    try
      var i: USize = 0
      while i < s.size() do
        if s(i)? == '.' then
          if not _valid_dec_octet(s, octet_start, i) then return false end
          octet_count = octet_count + 1
          octet_start = i + 1
        elseif not ((s(i)? >= '0') and (s(i)? <= '9')) then
          return false
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    if not _valid_dec_octet(s, octet_start, s.size()) then return false end
    octet_count = octet_count + 1
    octet_count == 4

  fun _valid_dec_octet(s: String val, from: USize, to: USize): Bool =>
    """
    0-255 with no leading zeros (except bare "0").
    """
    let len = to - from
    if (len == 0) or (len > 3) then return false end
    try
      if len == 1 then
        return (s(from)? >= '0') and (s(from)? <= '9')
      end
      // No leading zero for multi-digit
      if s(from)? == '0' then return false end
      var val': U16 = 0
      var i = from
      while i < to do
        let c = s(i)?
        if (c < '0') or (c > '9') then return false end
        val' = (val' * 10) + (c - '0').u16()
        i = i + 1
      end
      val' <= 255
    else
      _Unreachable()
      false
    end

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
