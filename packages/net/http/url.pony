primitive URLNoSlashes

type URLFormat is
  ( FormatDefault
  | URLNoSlashes
  )

class URL
  """
  Holds the components of a URL.
  """
  var scheme: String = ""
  var user: String = ""
  var password: String = ""
  var host: String = ""
  var service: String = ""
  var path: String = ""
  var query: String = ""
  var fragment: String = ""

  new val create(from: String) ? =>
    """
    Parse the URL string into its components.
    """
    // TODO: check character set, do % encoding and decoding
    var offset = _parse_scheme(from)
    offset = _parse_authority(from, offset)
    _parse_path(from, offset)

  fun string(fmt: URLFormat = FormatDefault,
    prefix: PrefixDefault = PrefixDefault, prec: U64 = -1, width: U64 = 0,
    align: Align = AlignLeft, fill: U32 = ' '): String iso^
  =>
    """
    Combine the components into a string.
    """
    let len = scheme.size() + 3 + user.size() + 1 + password.size() + 1 +
      host.size() + 1 + service.size() + path.size() + 1 + query.size() + 1 +
      fragment.size()
    let s = recover String(len) end

    if scheme.size() > 0 then
      s.append(scheme)
      s.append(":")
    end

    if
      (fmt isnt URLNoSlashes) and
      ((scheme.size() > 0) or (user.size() > 0) or (host.size() > 0))
    then
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

      if service.size() > 0 then
        s.append(":")
        s.append(service)
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

  fun join(that: URL): this->URL =>
    """
    Using this as a base URL, concatenate with another, possibly relative, URL
    in the same way a browser does for a link.
    """
    // TODO:
    this

  fun ref _parse_scheme(from: String): I64 ? =>
    """
    The scheme is: [a-zA-Z][a-zA-Z0-9+-.]*
    """
    var i = I64(0)

    while i < from.size().i64() do
      let c = from(i)

      if (c >= 'a') and (c <= 'Z') then
        // Valid any time.
        i = i + 1
      elseif
        ((c >= '0') and (c <= '9')) or
        (c == '+') or
        (c == '-') or
        (c == '.')
      then
        // If it's the first character, it's not a scheme.
        if i == 0 then
          return 0
        end
      elseif c == ':' then
        // Can't start a URL with a colon.
        if i == 0 then
          error
        end

        // Otherwise, we have a scheme. Set it and return the offset.
        scheme = recover from.substring(0, i - 1).lower_in_place() end
        return i + 1
      end

      i = i + 1
    end

    // The whole url is a valid scheme. Treat it as a path instead.
    0

  fun ref _parse_authority(from: String, offset: I64): I64 =>
    """
    The authority is: //userinfo@host:port/
    If the leading // isn't present, go straight to the path.
    """
    if not from.at("//", offset) then
      return offset
    end

    var i = offset + 2
    let slash = try from.find("/", i) else from.size().i64() end

    try
      let at = from.rfind("@", slash)

      try
        let colon = from.rfind(":", at)
        user = from.substring(i, colon - 1)
        password = from.substring(colon + 1, at - 1)
      else
        // Has no password.
        user = from.substring(i, at - 1)
      end

      i = at + 1
    end

    try
      let colon = from.find(":", i)
      host = from.substring(i, colon - 1)
      service = from.substring(colon + 1, slash - 1)
    else
      // Has no port
      host = from.substring(i, slash - 1)
    end

    slash

  fun ref _parse_path(from: String, offset: I64) =>
    """
    The remainder is path[?query][#fragment]
    """
    try
      let q = from.find("?", offset)
      path = from.substring(offset, q - 1)

      try
        let h = from.find("#", q + 1)
        query = from.substring(q + 1, h - 1)
        fragment = from.substring(h + 1, -1)
      else
        // Has no fragment
        query = from.substring(q + 1, -1)
      end
    else
      // Has no query.
      try
        let h = from.find("#", offset + 1)
        path = from.substring(offset, h - 1)
        fragment = from.substring(h + 1, -1)
      else
        // Has no fragment
        path = from.substring(offset, -1)
      end
    end

    if (path.size() == 0) and ((scheme.size() > 0) or (host.size() > 0)) then
      path = "/"
    end

  fun tag _hex(value: U8): U8 ? =>
    """
    Treat an ASCII byte as a hex value.
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
