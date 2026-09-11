primitive IRIPercentEncode
  """
  IRI-aware percent-encoding for constructing IRIs from unencoded text.

  Like `PercentEncode`, but preserves non-ASCII characters that are allowed
  literally in IRIs per RFC 3987: `ucschar` codepoints in all components,
  plus `iprivate` codepoints in the query component. Non-ASCII characters
  outside these ranges are percent-encoded byte-by-byte.

  ASCII encoding rules are identical to `PercentEncode` — the `URIPart`
  parameter selects the same component-specific allowed characters.
  """
  fun apply(input: String val, part: URIPart): String val =>
    """
    Percent-encode `input` for the given URI component, preserving
    IRI-legal non-ASCII codepoints.
    """
    let allow_iprivate =
      match part
      | URIPartQuery => true
      else false
      end

    let out = String(input.size())
    var i: USize = 0
    while i < input.size() do
      try
        let c = input(i)?
        if c < 0x80 then
          // ASCII: use standard URI encoding rules
          if PercentEncode._allowed(c, part) then
            out.push(c)
          else
            out.append(PercentEncode._encode_byte(c))
          end
          i = i + 1
        else
          // Non-ASCII: check if the UTF-8 sequence is an allowed IRI char
          (let cp, let cp_len) = input.utf32(i.isize())?
          let byte_len = cp_len.usize()
          if _is_allowed_iri(cp, allow_iprivate) then
            // Emit the raw UTF-8 bytes
            var j: USize = 0
            while j < byte_len do
              out.push(input(i + j)?)
              j = j + 1
            end
          else
            // Percent-encode each UTF-8 byte
            var j: USize = 0
            while j < byte_len do
              out.append(PercentEncode._encode_byte(input(i + j)?))
              j = j + 1
            end
          end
          i = i + byte_len
        end
      else
        _Unreachable()
      end
    end
    out.clone()

  fun _is_allowed_iri(cp: U32, allow_iprivate: Bool): Bool =>
    // RFC 3987 section 4.1: bidi formatting chars must stay encoded
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
