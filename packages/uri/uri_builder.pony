class ref URIBuilder
  """
  Fluent builder for constructing URIs from raw (unencoded) components.

  Each setter accepts raw text and automatically percent-encodes it for the
  target component. The `build()` method validates the assembled URI and
  returns a `URI val`.

  ```pony
  match URIBuilder
    .set_scheme("https")
    .set_host("example.com")
    .set_path("/api/users")
    .add_query_param("name", "Jane Doe")
    .set_fragment("top")
    .build()
  | let u: URI val =>
    // u.string() == "https://example.com/api/users?name=Jane%20Doe#top"
  | let e: URIBuildError val =>
    // handle error
  end
  ```

  Use `from()` to copy an existing URI's pre-encoded components into the
  builder for modification. Since `from()` copies pre-encoded values, do not
  pass an existing URI's encoded component (e.g., `uri.query`) to a setter
  like `set_query()` — that would double-encode it. Instead, use `from()`
  and then modify only the components you want to change.
  """
  var _scheme: (String | None) = None
  var _userinfo: (String | None) = None
  var _host: (String | None) = None
  var _port: (U16 | None) = None
  var _path: String = ""
  var _query: (String | None) = None
  var _fragment: (String | None) = None

  new create() =>
    """
    Create an empty builder with all components unset.
    """
    None

  new from(uri: URI val) =>
    """
    Copy pre-encoded components from an existing URI.

    Authority components (userinfo, host, port) are decomposed from the URI's
    authority. All values are copied as-is without re-encoding.
    """
    _scheme = uri.scheme
    match uri.authority
    | let a: URIAuthority =>
      _userinfo = a.userinfo
      _host = a.host
      _port = a.port
    end
    _path = uri.path
    _query = uri.query
    _fragment = uri.fragment

  fun ref set_scheme(scheme: String): URIBuilder ref =>
    """
    Set the URI scheme from raw text.

    The scheme is validated on `build()` — it must match
    `[A-Za-z][A-Za-z0-9+-.]*` per RFC 3986 section 3.1. No encoding is
    needed for schemes (they are ASCII-only).
    """
    _scheme = scheme
    this

  fun ref clear_scheme(): URIBuilder ref =>
    """
    Remove the scheme component.
    """
    _scheme = None
    this

  fun ref set_userinfo(userinfo: String): URIBuilder ref =>
    """
    Set the userinfo component from raw (unencoded) text.

    The input is percent-encoded for the userinfo component. If no host is
    set, an empty host is created automatically (userinfo requires an
    authority).
    """
    _userinfo = PercentEncode(userinfo, URIPartUserinfo)
    if _host is None then _host = "" end
    this

  fun ref clear_userinfo(): URIBuilder ref =>
    """
    Remove the userinfo component.
    """
    _userinfo = None
    this

  fun ref set_host(host: String): URIBuilder ref =>
    """
    Set the host component from raw (unencoded) text.

    If the input starts with `[`, it is treated as an IP-literal and stored
    as-is (validated on `build()`). Otherwise, it is percent-encoded as a
    reg-name. IPv6 addresses must include brackets (e.g., `[::1]`).
    """
    if (host.size() > 0) and try host(0)? == '[' else false end then
      _host = host
    else
      _host = PercentEncode(host, URIPartHost)
    end
    this

  fun ref clear_host(): URIBuilder ref =>
    """
    Remove the host component.

    Also clears userinfo and port, since they require an authority.
    """
    _host = None
    _userinfo = None
    _port = None
    this

  fun ref set_port(port: U16): URIBuilder ref =>
    """
    Set the port number.

    If no host is set, an empty host is created automatically (port requires
    an authority).
    """
    _port = port
    if _host is None then _host = "" end
    this

  fun ref clear_port(): URIBuilder ref =>
    """
    Remove the port component.
    """
    _port = None
    this

  fun ref set_path(path: String): URIBuilder ref =>
    """
    Set the path component from raw (unencoded) text.

    The input is percent-encoded for the path component. Path structure
    (slashes) is preserved — only characters that need encoding are encoded.
    """
    _path = PercentEncode(path, URIPartPath)
    this

  fun ref append_path_segment(segment: String): URIBuilder ref =>
    """
    Append a single path segment from raw (unencoded) text.

    The segment is encoded with slash (`/`) also encoded (as `%2F`) so
    it remains a single segment. A `/` separator is prepended automatically.
    """
    let encoded = _PathSegmentEncode(segment)
    _path =
      recover val
        String
          .> append(_path)
          .> push('/')
          .> append(encoded)
      end
    this

  fun ref set_query(query: String): URIBuilder ref =>
    """
    Set the query component from raw (unencoded) text.

    The input is percent-encoded for the query component. Structure characters
    `&` and `=` are preserved since they are sub-delimiters allowed in query.
    For individual key-value pairs with full encoding, use `add_query_param`.
    """
    _query = PercentEncode(query, URIPartQuery)
    this

  fun ref clear_query(): URIBuilder ref =>
    """
    Remove the query component.

    After clearing, no `?` delimiter appears in the output. To produce a
    trailing `?` with no value, use `set_query("")` instead.
    """
    _query = None
    this

  fun ref add_query_param(key: String, value: String): URIBuilder ref =>
    """
    Add a query parameter from raw (unencoded) key and value.

    Both the key and value are encoded with `=`, `&`, and `+` also encoded
    (in addition to standard query encoding) so they are safe as individual
    parameter components. Parameters are appended with `&` as separator.
    """
    let encoded_key = _FormFieldEncode(key)
    let encoded_val = _FormFieldEncode(value)
    let param =
      recover val
        String
          .> append(encoded_key)
          .> push('=')
          .> append(encoded_val)
      end
    _query =
      match \exhaustive\ _query
      | None => param
      | let q: String if q.size() == 0 => param
      | let q: String =>
        recover val
          String
            .> append(q)
            .> push('&')
            .> append(param)
        end
      end
    this

  fun ref set_fragment(fragment: String): URIBuilder ref =>
    """
    Set the fragment component from raw (unencoded) text.

    The input is percent-encoded for the fragment component.
    """
    _fragment = PercentEncode(fragment, URIPartFragment)
    this

  fun ref clear_fragment(): URIBuilder ref =>
    """
    Remove the fragment component.

    After clearing, no `#` delimiter appears in the output. To produce a
    trailing `#` with no value, use `set_fragment("")` instead.
    """
    _fragment = None
    this

  fun box build(): (URI val | URIBuildError val) =>
    """
    Validate and assemble the URI from the current components.

    Returns `InvalidScheme` if the scheme is set but does not match
    `[A-Za-z][A-Za-z0-9+-.]*`. Returns `InvalidHost` if an IP-literal
    host is malformed. Returns the assembled `URI val` on success.
    """
    // Validate scheme if present
    match _scheme
    | let s: String =>
      if not _valid_scheme(s) then
        return InvalidScheme
      end
    end

    // Build authority if host is present
    let authority: (URIAuthority | None) =
      match \exhaustive\ _host
      | let h: String =>
        // Assemble authority string for validation via ParseURIAuthority
        let auth_str =
          recover val
            let out = String
            match _userinfo
            | let u: String => out.append(u); out.push('@')
            end
            out.append(h)
            match _port
            | let p: U16 => out.push(':'); out.append(p.string())
            end
            out
          end
        match \exhaustive\ ParseURIAuthority(auth_str)
        | let a: URIAuthority val => a
        | let e: URIParseError val => return e
        end
      | None => None
      end

    // Auto-prepend "/" if authority present and path is non-empty but relative
    let path: String val =
      match authority
      | let _: URIAuthority =>
        if (_path.size() > 0) and
          (try _path(0)? != '/' else false end)
        then
          recover val
            String
              .> push('/')
              .> append(_path)
          end
        else
          _path
        end
      else
        _path
      end

    URI(_scheme, authority, path, _query, _fragment)

  fun _valid_scheme(s: String): Bool =>
    if s.size() == 0 then return false end
    try
      let first = s(0)?
      if not (((first >= 'A') and (first <= 'Z')) or
        ((first >= 'a') and (first <= 'z')))
      then
        return false
      end
    else
      _Unreachable()
    end
    try
      var i: USize = 1
      while i < s.size() do
        let c = s(i)?
        if not (((c >= 'A') and (c <= 'Z')) or
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
