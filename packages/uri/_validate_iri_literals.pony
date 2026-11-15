primitive _ValidateIRILiterals
  """
  Check a URI component string for literal non-ASCII characters that are
  not allowed in an IRI per RFC 3987.

  Returns `true` when every literal non-ASCII codepoint is legal for the
  component. Returns `false` when any byte >= 0x80 is part of an invalid
  UTF-8 sequence, or decodes to a codepoint that is outside the allowed
  set or is a bidi formatting character.
  """
  fun is_valid(s: String val, allow_iprivate: Bool): Bool =>
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if c < 0x80 then
          i = i + 1
        else
          (let cp, let cp_len) = s.utf32(i.isize())?
          let byte_len = cp_len.usize()
          if byte_len == 0 then
            return false
          end
          if not _is_legal_iri_literal(cp, allow_iprivate) then
            return false
          end
          i = i + byte_len
        end
      else
        return false
      end
    end
    true

  fun _is_legal_iri_literal(cp: U32, allow_iprivate: Bool): Bool =>
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
