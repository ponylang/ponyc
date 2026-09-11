primitive ParseURI
  """
  Parse a URI-reference (RFC 3986 section 4.1) into structured components.

  Handles both absolute URIs (`http://host/path?query`) and relative
  references (`/path?query`). Components are stored percent-encoded.

  The parsing algorithm follows RFC 3986 section 3: scan for `:` before
  any of `/?#` to identify the scheme, then check for `//` to identify
  authority, then extract path (up to `?` or `#`), query (between `?`
  and `#`), and fragment (after `#`). Scanning operates on literal
  delimiter characters only — percent-encoded delimiters (`%2F`, `%3F`,
  `%23`) are not treated as structural boundaries.

  Scheme validation: the scheme must start with a letter and contain
  only letters, digits, `+`, `-`, and `.` (RFC 3986 section 3.1).

  The asterisk-form (`*`) used by HTTP OPTIONS is a valid relative
  reference under RFC 3986 — it parses as a URI with path `*` and no
  other components.
  """
  fun apply(raw: String val): (URI val | URIParseError val) =>
    """
    Parse `raw` into a `URI`, or return a `URIParseError` on failure.
    """
    var pos: USize = 0
    let len = raw.size()

    // Step 1: Detect scheme by scanning for ':' before any of '/?#'
    var scheme: (String | None) = None
    try
      var i: USize = 0
      while i < len do
        let c = raw(i)?
        if c == ':' then
          // Everything before ':' is potential scheme
          let candidate: String val = raw.substring(0, i.isize())
          if _valid_scheme(candidate) then
            scheme = candidate
            pos = i + 1
          end
          break
        elseif (c == '/') or (c == '?') or (c == '#') then
          break
        end
        i = i + 1
      end
    else
      _Unreachable()
    end

    // Step 2: Check for authority (starts with "//")
    var authority: (URIAuthority | None) = None
    try
      if ((pos + 1) < len) and (raw(pos)? == '/') and (raw(pos + 1)? == '/')
      then
        pos = pos + 2
        // Find end of authority: next '/', '?', or '#'
        var auth_end = pos
        while auth_end < len do
          let c = raw(auth_end)?
          if (c == '/') or (c == '?') or (c == '#') then
            break
          end
          auth_end = auth_end + 1
        end
        match \exhaustive\ ParseURIAuthority._parse(raw, pos, auth_end)
        | let a: URIAuthority val =>
          authority = a
        | let e: URIParseError val =>
          return e
        end
        pos = auth_end
      end
    else
      _Unreachable()
    end

    // Step 3: Extract path (up to '?' or '#')
    var path_end = pos
    try
      while path_end < len do
        let c = raw(path_end)?
        if (c == '?') or (c == '#') then
          break
        end
        path_end = path_end + 1
      end
    else
      _Unreachable()
    end
    let path: String val = raw.substring(pos.isize(), path_end.isize())
    pos = path_end

    // Step 4: Extract query (between '?' and '#')
    var query: (String | None) = None
    try
      if (pos < len) and (raw(pos)? == '?') then
        pos = pos + 1
        var query_end = pos
        while query_end < len do
          if raw(query_end)? == '#' then
            break
          end
          query_end = query_end + 1
        end
        let q: String val = raw.substring(pos.isize(), query_end.isize())
        query = q
        pos = query_end
      end
    else
      _Unreachable()
    end

    // Step 5: Extract fragment (after '#')
    var fragment: (String | None) = None
    try
      if (pos < len) and (raw(pos)? == '#') then
        pos = pos + 1
        let f: String val = raw.substring(pos.isize(), len.isize())
        fragment = f
      end
    else
      _Unreachable()
    end

    URI(scheme, authority, path, query, fragment)

  fun _valid_scheme(candidate: String val): Bool =>
    """
    RFC 3986 section 3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    """
    if candidate.size() == 0 then
      return false
    end
    try
      let first = candidate(0)?
      if not (((first >= 'A') and (first <= 'Z')) or
        ((first >= 'a') and (first <= 'z')))
      then
        return false
      end
      var i: USize = 1
      while i < candidate.size() do
        let c = candidate(i)?
        if not (
          ((c >= 'A') and (c <= 'Z')) or
            ((c >= 'a') and (c <= 'z')) or
            ((c >= '0') and (c <= '9')) or
            (c == '+') or (c == '-') or (c == '.'))
        then
          return false
        end
        i = i + 1
      end
    else
      _Unreachable()
    end
    true
