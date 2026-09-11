primitive URIToIRI
  """
  Convert a URI to an IRI by selectively decoding percent-encoded UTF-8
  sequences that represent allowed IRI characters.

  Percent-encoded sequences that decode to valid UTF-8 `ucschar` codepoints
  (RFC 3987 section 2.2) are replaced with their literal UTF-8 bytes. In the
  query component, `iprivate` codepoints are also decoded. Bidi formatting
  characters (U+200E, U+200F, U+202A-202E) always stay percent-encoded per
  RFC 3987 section 4.1.

  Sequences that don't form valid UTF-8, or decode to codepoints outside
  the allowed ranges, remain percent-encoded. Always succeeds.
  """
  fun apply(uri: URI val): URI val =>
    """
    Decode percent-encoded IRI-legal codepoints back to literal UTF-8.
    """
    let iri_authority: (URIAuthority | None) =
      match \exhaustive\ uri.authority
      | let a: URIAuthority =>
        let iri_userinfo: (String | None) =
          match \exhaustive\ a.userinfo
          | let u: String => _decode_iri(u, false)
          | None => None
          end
        URIAuthority(iri_userinfo, _decode_iri(a.host, false), a.port)
      | None => None
      end

    let iri_query: (String | None) =
      match \exhaustive\ uri.query
      | let q: String => _decode_iri(q, true)
      | None => None
      end

    let iri_fragment: (String | None) =
      match \exhaustive\ uri.fragment
      | let f: String => _decode_iri(f, false)
      | None => None
      end

    URI(
      uri.scheme,
      iri_authority,
      _decode_iri(uri.path, false),
      iri_query,
      iri_fragment)

  fun _decode_iri(s: String val, allow_iprivate: Bool): String val =>
    if not s.contains("%") then return s end

    let out = String(s.size())
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if (c == '%') and ((i + 2) < s.size()) then
          // Try to decode a UTF-8 sequence from percent-encoded bytes.
          // If hex decoding fails (non-hex digits), fall through to
          // pass the triplet through unchanged.
          var decoded = false
          try
            let first_byte = _decode_hex(s, i)?
            let seq_len = _utf8_sequence_length(first_byte)
            if (seq_len > 1) and ((i + (seq_len * 3)) <= s.size()) then
              let bytes = String(seq_len)
              bytes.push(first_byte)
              var j: USize = 1
              var valid = true
              while j < seq_len do
                let byte_offset = i + (j * 3)
                try
                  if s(byte_offset)? != '%' then
                    valid = false
                    break
                  end
                  bytes.push(_decode_hex(s, byte_offset)?)
                else
                  valid = false
                  break
                end
                j = j + 1
              end

              if valid and (bytes.size() == seq_len) then
                (let cp, let cp_len) = bytes.utf32(0)?
                if (cp_len.usize() == seq_len) and
                  _should_decode(cp, allow_iprivate)
                then
                  for b in bytes.values() do
                    out.push(b)
                  end
                  i = i + (seq_len * 3)
                  decoded = true
                end
              end
            end
          end
          if not decoded then
            // Keep the original %XX triplet (or malformed % sequence)
            out.push(s(i)?)
            out.push(s(i + 1)?)
            out.push(s(i + 2)?)
            i = i + 3
          end
        else
          out.push(c)
          i = i + 1
        end
      else
        _Unreachable()
      end
    end
    out.clone()

  fun _decode_hex(s: String val, offset: USize): U8 ? =>
    """
    Decode a single %HH triplet starting at offset.
    """
    let hi = PercentDecode._hex_value(s(offset + 1)?)?
    let lo = PercentDecode._hex_value(s(offset + 2)?)?
    (hi * 16) + lo

  fun _utf8_sequence_length(first_byte: U8): USize =>
    """
    Determine UTF-8 sequence length from the leading byte.
    """
    if (first_byte and 0x80) == 0 then 1
    elseif (first_byte and 0xE0) == 0xC0 then 2
    elseif (first_byte and 0xF0) == 0xE0 then 3
    elseif (first_byte and 0xF8) == 0xF0 then 4
    else 1 // Invalid leading byte — treat as single byte
    end

  fun _should_decode(cp: U32, allow_iprivate: Bool): Bool =>
    """
    Check if a codepoint should be decoded to literal UTF-8.
    """
    if _IRIChars.is_bidi_format(cp) then
      return false
    end
    if _IRIChars.is_ucschar(cp) then
      return true
    end
    if allow_iprivate and _IRIChars.is_iprivate(cp) then
      return true
    end
    false
