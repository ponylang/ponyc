primitive IRIToURI
  """
  Convert an IRI to a URI by percent-encoding all non-ASCII bytes.

  Each component is processed byte-by-byte: bytes >= 0x80 become `%HH`,
  existing `%XX` triplets pass through unchanged, and ASCII bytes pass
  through unchanged. The scheme component is always ASCII per RFC 3986
  and is passed through without modification.

  Always succeeds — any byte sequence is valid input.
  """
  fun apply(iri: URI val): URI val =>
    """
    Percent-encode non-ASCII bytes in each IRI component to produce a
    URI.
    """
    let uri_authority: (URIAuthority | None) =
      match \exhaustive\ iri.authority
      | let a: URIAuthority =>
        let uri_userinfo: (String | None) =
          match \exhaustive\ a.userinfo
          | let u: String => _encode(u)
          | None => None
          end
        URIAuthority(uri_userinfo, _encode(a.host), a.port)
      | None => None
      end

    let uri_query: (String | None) =
      match \exhaustive\ iri.query
      | let q: String => _encode(q)
      | None => None
      end

    let uri_fragment: (String | None) =
      match \exhaustive\ iri.fragment
      | let f: String => _encode(f)
      | None => None
      end

    URI(
      iri.scheme,
      uri_authority,
      _encode(iri.path),
      uri_query,
      uri_fragment)

  fun _is_hex(c: U8): Bool =>
    ((c >= '0') and (c <= '9')) or
      ((c >= 'A') and (c <= 'F')) or
      ((c >= 'a') and (c <= 'f'))

  fun _encode(s: String val): String val =>
    var has_non_ascii: Bool = false
    for c in s.values() do
      if c >= 0x80 then
        has_non_ascii = true
        break
      end
    end
    if not has_non_ascii then return s end

    let out = String(s.size() * 2)
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if (c == '%') and ((i + 2) < s.size()) and
          _is_hex(s(i + 1)?) and _is_hex(s(i + 2)?)
        then
          // Pass through existing percent-encoded triplets
          out.push(c)
          out.push(s(i + 1)?)
          out.push(s(i + 2)?)
          i = i + 3
        elseif c >= 0x80 then
          out.append(PercentEncode._encode_byte(c))
          i = i + 1
        else
          out.push(c)
          i = i + 1
        end
      else
        _Unreachable()
      end
    end
    out.clone()
