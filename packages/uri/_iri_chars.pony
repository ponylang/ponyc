primitive _IRIChars
  """
  Codepoint classification for IRI characters per RFC 3987.

  `ucschar` (section 2.2): allowed in IRI components except scheme.
  `iprivate` (section 2.2): allowed only in the query component.
  Bidi formatting characters: within the `ucschar` BMP range but
  prohibited from appearing literally in IRIs (section 4.1).
  """
  fun is_ucschar(cp: U32): Bool =>
    // RFC 3987 section 2.2:
    // ucschar = %xA0-D7FF / %xF900-FDCF / %xFDF0-FFEF
    // / %x10000-1FFFD / %x20000-2FFFD / %x30000-3FFFD
    // / %x40000-4FFFD / %x50000-5FFFD / %x60000-6FFFD
    // / %x70000-7FFFD / %x80000-8FFFD / %x90000-9FFFD
    // / %xA0000-AFFFD / %xB0000-BFFFD / %xC0000-CFFFD
    // / %xD0000-DFFFD / %xE1000-EFFFD

    // BMP ranges
    if (cp >= 0xA0) and (cp <= 0xD7FF) then
      return true
    end
    if (cp >= 0xF900) and (cp <= 0xFDCF) then
      return true
    end
    if (cp >= 0xFDF0) and (cp <= 0xFFEF) then
      return true
    end

    // Planes 1-13: each plane allows x0000-xFFFD (excludes xFFFE-xFFFF)
    if (cp >= 0x10000) and (cp <= 0xDFFFD) then
      return (cp and 0xFFFF) <= 0xFFFD
    end

    // Plane 14 partial: E1000-EFFFD
    if (cp >= 0xE1000) and (cp <= 0xEFFFD) then
      return true
    end

    false

  fun is_iprivate(cp: U32): Bool =>
    // RFC 3987 section 2.2:
    // iprivate = %xE000-F8FF / %xF0000-FFFFD / %x100000-10FFFD
    if (cp >= 0xE000) and (cp <= 0xF8FF) then
      return true
    end
    if (cp >= 0xF0000) and (cp <= 0xFFFFD) then
      return true
    end
    if (cp >= 0x100000) and (cp <= 0x10FFFD) then
      return true
    end
    false

  fun is_bidi_format(cp: U32): Bool =>
    // RFC 3987 section 4.1 prohibits these from appearing literally in IRIs:
    // U+200E (LRM), U+200F (RLM), U+202A-202E (LRE, RLE, PDF, LRO, RLO)
    if (cp == 0x200E) or (cp == 0x200F) then
      return true
    end
    (cp >= 0x202A) and (cp <= 0x202E)
