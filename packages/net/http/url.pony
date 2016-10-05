class val URL
  """
  Holds the components of a URL. These are always stored as valid, URL-encoded
  values.
  """
  var scheme: String = ""
  var user: String = ""
  var password: String = ""
  var host: String = ""
  var port: U16 = 0
  var path: String = ""
  var query: String = ""
  var fragment: String = ""

  new val create() =>
    """
    Create an empty URL.
    """
    None

  new val build(from: String, percent_encoded: Bool = true) ? =>
    """
    Parse the URL string into its components. If it isn't URL encoded, encode
    it. If existing URL encoding is invalid, raise an error.
    """
    _parse(from)

    if not URLEncode.check_scheme(scheme) then error end
    user = URLEncode.encode(user, URLPartUser, percent_encoded)
    password = URLEncode.encode(password, URLPartPassword, percent_encoded)
    host = URLEncode.encode(host, URLPartHost, percent_encoded)
    path = URLEncode.encode(path, URLPartPath, percent_encoded)
    query = URLEncode.encode(query, URLPartQuery, percent_encoded)
    fragment = URLEncode.encode(fragment, URLPartFragment, percent_encoded)

  new val valid(from: String) ? =>
    """
    Parse the URL string into its components. If it isn't URL encoded, raise an
    error.
    """
    _parse(from)

    if not is_valid() then
      error
    end

  fun is_valid(): Bool =>
    """
    Return true if all elements are correctly URL encoded.
    """
    URLEncode.check_scheme(scheme) and
      URLEncode.check(user, URLPartUser) and
      URLEncode.check(password, URLPartPassword) and
      URLEncode.check(host, URLPartHost) and
      URLEncode.check(path, URLPartPath) and
      URLEncode.check(query, URLPartQuery) and
      URLEncode.check(fragment, URLPartFragment)

  fun string(): String iso^ =>
    """
    Combine the components into a string.
    """
    let len = scheme.size() + 3 + user.size() + 1 + password.size() + 1 +
      host.size() + 6 + path.size() + 1 + query.size() + 1 + fragment.size()
    let s = recover String(len) end

    if scheme.size() > 0 then
      s.append(scheme)
      s.append(":")
    end

    if (user.size() > 0) or (host.size() > 0) then
      s.append("//")
    end

    if user.size() > 0 then
      s.append(user)

      if password.size() > 0 then
        s.append(":")
        s.append(password)
      end

      s.append("@")
    end

    if host.size() > 0 then
      s.append(host)

      // Do not output port if it's the scheme default.
      if port != default_port() then
        s.append(":")
        s.append(port.string())
      end
    end

    s.append(path)

    if query.size() > 0 then
      s.append("?")
      s.append(query)
    end

    if fragment.size() > 0 then
      s.append("#")
      s.append(fragment)
    end

    consume s

  fun val join(that: URL): URL =>
    """
    Using this as a base URL, concatenate with another, possibly relative, URL
    in the same way a browser does for a link.
    """
    // TODO:
    this

  fun default_port(): U16 =>
    """
    Report the default port for our scheme.
    Returns 0 for unrecognised schemes.
    """
    match scheme
    | "http" => 80
    | "https" => 443
    else
      0
    end

  fun ref _parse(from: String) ? =>
    """
    Parse the given string as a URL.
    Raises an error on invalid port number.
    """
    (var offset, scheme) = _parse_scheme(from)
    (offset, let authority) = _parse_part(from, "//", "/?#", offset)
    (offset, path) = _parse_part(from, "", "?#", offset)
    (offset, query) = _parse_part(from, "?", "#", offset)
    (offset, fragment) = _parse_part(from, "#", "", offset)

    if path.size() == 0 then
      // An empty path is a root path.
      path = "/"
    end

    (var userinfo, var hostport) = _split(authority, '@')

    if hostport.size() == 0 then
      // No '@' found, hostport is whole of authority.
      hostport = userinfo = ""
    end

    (user, password) = _split(userinfo, ':')
    (host, var port_str) = _parse_hostport(hostport)

    port =
      if port_str.size() > 0 then
        port_str.u16()
      else
        default_port()
      end

  fun _parse_scheme(from: String): (/*offset*/ISize, /*scheme*/String) =>
    """
    Find the scheme, if any, at the start of the given string.
    The offset of the part following the scheme is returned.
    """
    // We have a scheme only if we have a ':' before any of "/?#".
    try
      var i = USize(0)

      while i < from.size() do
        let c = from(i)

        if c == ':' then
          // Scheme found.
          return ((i + 1).isize(), from.substring(0, i.isize()))
        end

        if (c == '/') or (c == '?') or (c == '#') then
          // No scheme.
          return (0, "")
        end

        i = i + 1
      end
    end

    // End of string reached without finding any relevant terminators.
    (0, "")

  fun _parse_part(from: String, prefix: String, terminators: String,
    offset: ISize): (/*offset*/ISize, /*part*/String)
  =>
    """
    Attempt to parse the specified part out of the given string.
    Only attempt the parse if the given prefix is found first. Pass "" if no
    prefix is needed.
    The part ends when any one of the given terminator characters is found, or
    the end of the input is reached.
    The offset of the terminator is returned, if one is found.
    """
    if (prefix.size() > 0) and (not from.at(prefix, offset)) then
      // Prefix not found.
      return (offset, "")
    end

    let start = offset + prefix.size().isize()

    try
      var i = start.usize()

      while i < from.size() do
        let c = from(i)

        var j = USize(0)
        while j < terminators.size() do
          if terminators(j) == c then
            // Terminator found.
            return (i.isize(), from.substring(start, i.isize()))
          end

          j = j + 1
        end

        i = i + 1
      end
    end

    // No terminator found, take whole string.
    (from.size().isize(), from.substring(start))

  fun _split(src: String, separator: U8): (String, String) =>
    """
    Split the given string in 2 around the first instance of the specified
    separator.
    If the string does not contain the separator then the first resulting
    string is the whole src and the second is empty.
    """
    try
      var i = USize(0)

      while i < src.size() do
        if src(i) == separator then
          // Separator found.
          return (src.substring(0, i.isize()), src.substring((i + 1).isize()))
        end

        i = i + 1
      end
    end

    // Separator not found.
    (src, "")

  fun _parse_hostport(hostport: String): (/*host*/String, /*port*/String) =>
    """
    Split the given "host and port" string into the host and port parts.
    """
    try
      if (hostport.size() == 0) or (hostport(0) != '[') then
        // This is not an IPv6 format host, just split at the first ':'.
        return _split(hostport, ':')
      end

      // This is an IPv6 format host, need to find the ']'
      var i = USize(0)
      var terminator = U8(']')

      while i < hostport.size() do
        if hostport(i) == terminator then
          if terminator == ':' then
            // ':' found, now we can separate the host and port
            return (hostport.substring(0, i.isize()),
              hostport.substring((i + 1).isize()))
          end

          // ']' found, now find ':'
          terminator = ':'
        end

        i = i + 1
      end
    end

    // ':' not found, we have no port.
    (hostport, "")
