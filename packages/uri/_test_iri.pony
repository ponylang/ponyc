use "pony_test"
use "pony_check"

// ============================================================================
// Property-based tests
// ============================================================================

class \nodoc\ iso _PropertyIRIToURINoNonASCII
  is Property1[String val]
  """
  IRIToURI output has no literal non-ASCII bytes in any component.
  """
  fun name(): String => "uri/iri_to_uri/no_non_ascii"

  fun gen(): Generator[String val] =>
    _IRIStringGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let iri: URI val =>
      let uri = IRIToURI(iri)
      let uri_str: String val = uri.string()
      for c in uri_str.values() do
        if c >= 0x80 then
          ph.fail("non-ASCII byte in IRIToURI output: " + uri_str +
            " from: " + arg1)
          return
        end
      end
    | let _: URIParseError val => None
    end

class \nodoc\ iso _PropertyURIToIRINoEncodedUcschar
  is Property1[String val]
  """
  URIToIRI output has no percent-encoded sequences that decode to ucschar.
  """
  fun name(): String => "uri/uri_to_iri/no_encoded_ucschar"

  fun gen(): Generator[String val] =>
    _URIWithEncodedNonASCIIGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let uri: URI val =>
      let iri = URIToIRI(uri)
      let iri_str: String val = iri.string()
      _check_no_encoded_ucschar(ph, iri_str)
    | let _: URIParseError val => None
    end

  fun _check_no_encoded_ucschar(
    ph: PropertyHelper,
    s: String val)
  =>
    var i: USize = 0
    while i < s.size() do
      try
        if s(i)? == '%' then
          if (i + 2) < s.size() then
            let first_byte =
              (PercentDecode._hex_value(s(i + 1)?)? * 16) +
                PercentDecode._hex_value(s(i + 2)?)?
            let seq_len = URIToIRI._utf8_sequence_length(first_byte)
            if (seq_len > 1) and ((i + (seq_len * 3)) <= s.size()) then
              let bytes = String(seq_len)
              bytes.push(first_byte)
              var j: USize = 1
              var valid = true
              while j < seq_len do
                let offset = i + (j * 3)
                if s(offset)? != '%' then
                  valid = false
                  break
                end
                bytes.push(
                  (PercentDecode._hex_value(s(offset + 1)?)? * 16) +
                    PercentDecode._hex_value(s(offset + 2)?)?)
                j = j + 1
              end
              if valid and (bytes.size() == seq_len) then
                try
                  (let cp, let cp_len) = bytes.utf32(0)?
                  if (cp_len.usize() == seq_len) and
                    _IRIChars.is_ucschar(cp) and
                    (not _IRIChars.is_bidi_format(cp))
                  then
                    ph.fail(
                      "encoded ucschar U+" +
                        _hex_u32(cp) + " in: " + s)
                    return
                  end
                end
              end
            end
          end
          i = i + 3
        else
          i = i + 1
        end
      else
        return
      end
    end

  fun _hex_u32(cp: U32): String val =>
    let hex = "0123456789ABCDEF"
    recover val
      let out = String(6)
      if cp > 0xFFFF then
        try
          out.push(hex((cp >> 20).usize() and 0x0F)?)
          out.push(hex((cp >> 16).usize() and 0x0F)?)
        else _Unreachable() end
      end
      if cp > 0xFF then
        try
          out.push(hex((cp >> 12).usize() and 0x0F)?)
          out.push(hex((cp >> 8).usize() and 0x0F)?)
        else _Unreachable() end
      end
      try
        out.push(hex((cp >> 4).usize() and 0x0F)?)
        out.push(hex(cp.usize() and 0x0F)?)
      else _Unreachable() end
      out
    end

class \nodoc\ iso _PropertyIRIToURIIdempotent
  is Property1[String val]
  """
  Applying IRIToURI twice produces the same result as once.
  """
  fun name(): String => "uri/iri_to_uri/idempotent"

  fun gen(): Generator[String val] =>
    _IRIStringGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let iri: URI val =>
      let once = IRIToURI(iri)
      let twice = IRIToURI(once)
      ph.assert_true(
        once == twice,
        "not idempotent: once=" + once.string() +
          " twice=" + twice.string() +
          " input=" + arg1)
    | let _: URIParseError val => None
    end

class \nodoc\ iso _PropertyURIToIRIIdempotent
  is Property1[String val]
  """
  Applying URIToIRI twice produces the same result as once.
  """
  fun name(): String => "uri/uri_to_iri/idempotent"

  fun gen(): Generator[String val] =>
    _URIWithEncodedNonASCIIGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let uri: URI val =>
      let once = URIToIRI(uri)
      let twice = URIToIRI(once)
      ph.assert_true(
        once == twice,
        "not idempotent: once=" + once.string() +
          " twice=" + twice.string() +
          " input=" + arg1)
    | let _: URIParseError val => None
    end

class \nodoc\ iso _PropertyIRIToURIRoundtrip
  is Property1[String val]
  """
  For IRIs containing only ucschar non-ASCII codepoints (no iprivate),
  URIToIRI(IRIToURI(iri)) produces the original URI structure.
  """
  fun name(): String => "uri/iri/roundtrip"

  fun gen(): Generator[String val] =>
    _IRIUcscharOnlyGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let iri: URI val =>
      let uri_form = IRIToURI(iri)
      let back = URIToIRI(uri_form)
      ph.assert_true(
        iri == back,
        "roundtrip failed: original=" + iri.string() +
          " uri=" + uri_form.string() +
          " back=" + back.string())
    | let _: URIParseError val => None
    end

class \nodoc\ iso _PropertyNormalizeIRIIdempotent
  is Property1[_NormalizableURIInput]
  """
  Normalizing an already-normalized IRI produces the same IRI.
  """
  fun name(): String => "uri/normalize_iri/idempotent"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    let uri = arg1.uri
    match \exhaustive\ NormalizeIRI(uri)
    | let once: URI val =>
      match \exhaustive\ NormalizeIRI(once)
      | let twice: URI val =>
        ph.assert_true(
          once == twice,
          "not idempotent: once=" + once.string() +
            " twice=" + twice.string() +
            " original=" + uri.string())
      | let e: InvalidPercentEncoding val =>
        ph.fail("second normalization failed for: " + once.string())
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("first normalization failed for: " + uri.string())
    end

class \nodoc\ iso _PropertyIRIEquivalentReflexive
  is Property1[_NormalizableURIInput]
  """
  Every valid URI/IRI is equivalent to itself.
  """
  fun name(): String => "uri/iri_equivalent/reflexive"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    let uri = arg1.uri
    match \exhaustive\ IRIEquivalent(uri, uri)
    | let result: Bool =>
      ph.assert_true(
        result, "not reflexive: " + uri.string())
    | let e: InvalidPercentEncoding val =>
      ph.fail("equivalence failed for: " + uri.string())
    end

class \nodoc\ iso _PropertyIRIEquivalentCrossForms
  is Property1[String val]
  """
  An IRI and its URI form (via IRIToURI) are equivalent.
  """
  fun name(): String => "uri/iri_equivalent/cross_forms"

  fun gen(): Generator[String val] =>
    _IRIUcscharOnlyGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let iri: URI val =>
      let uri_form = IRIToURI(iri)
      match \exhaustive\ IRIEquivalent(iri, uri_form)
      | let result: Bool =>
        ph.assert_true(
          result,
          "IRI and URI form not equivalent: iri=" + iri.string() +
            " uri=" + uri_form.string())
      | let e: InvalidPercentEncoding val =>
        ph.fail("equivalence failed: " + iri.string())
      end
    | let _: URIParseError val => None
    end

class \nodoc\ iso _PropertyIRIPercentEncodePreservesUcschar
  is Property1[String val]
  """
  IRIPercentEncode preserves ucschar codepoints as literal UTF-8.
  """
  fun name(): String => "uri/iri_percent_encode/preserves_ucschar"

  fun gen(): Generator[String val] =>
    _UcscharStringGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let encoded = IRIPercentEncode(arg1, URIPartPath)
    // Count non-ASCII codepoints in input and output — ucschar should
    // survive as literal UTF-8, so the count must match.
    let input_ucschar = _count_non_ascii_codepoints(arg1)
    let output_ucschar = _count_non_ascii_codepoints(encoded)
    ph.assert_eq[USize](
      input_ucschar,
      output_ucschar,
      "ucschar count mismatch: input has " + input_ucschar.string() +
        " non-ASCII codepoints but output has " +
        output_ucschar.string() +
        " input=" + arg1 + " output=" + encoded)

  fun _count_non_ascii_codepoints(s: String val): USize =>
    var count: USize = 0
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if c >= 0x80 then
          (let cp, let cp_len) = s.utf32(i.isize())?
          count = count + 1
          i = i + cp_len.usize()
        else
          i = i + 1
        end
      else
        return count
      end
    end
    count

class \nodoc\ iso _PropertyIRIPercentEncodeEncodesNonAllowed
  is Property1[String val]
  """
  IRIPercentEncode encodes non-ASCII characters outside ucschar/iprivate.
  """
  fun name(): String => "uri/iri_percent_encode/encodes_non_allowed"

  fun gen(): Generator[String val] =>
    // Characters that should NOT appear literally in IRIs:
    // U+0080-009F (between ASCII and ucschar start at U+00A0)
    // U+200E-200F, U+202A-202E (bidi formatting — ucschar but prohibited)
    Generators.one_of[String val](
      [
        "\x80"    // U+0080 (control char)
        "\x9F"    // U+009F (last control before ucschar)
        "\u200E"  // U+200E (LRM — bidi formatting)
        "\u200F"  // U+200F (RLM — bidi formatting)
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let encoded = IRIPercentEncode(arg1, URIPartPath)
    // All non-ASCII bytes should be percent-encoded
    for c in encoded.values() do
      if c >= 0x80 then
        ph.fail("non-ASCII byte in output for non-ucschar input: " + encoded +
          " from: " + arg1)
        return
      end
    end

// ============================================================================
// Example-based tests
// ============================================================================
class \nodoc\ iso _TestIRICharsBoundary is UnitTest
  """
  Boundary tests for _IRIChars codepoint classification.
  """
  fun name(): String => "uri/iri_chars/boundary"

  fun ref apply(h: TestHelper) =>
    // -- ucschar boundaries --
    // U+009F just below range
    h.assert_false(
      _IRIChars.is_ucschar(0x9F), "U+009F should not be ucschar")
    // U+00A0 start of range
    h.assert_true(
      _IRIChars.is_ucschar(0xA0), "U+00A0 should be ucschar")
    // U+D7FF end of first BMP range
    h.assert_true(
      _IRIChars.is_ucschar(0xD7FF), "U+D7FF should be ucschar")
    // U+D800 just past first BMP range (surrogate)
    h.assert_false(
      _IRIChars.is_ucschar(0xD800), "U+D800 should not be ucschar")

    // U+F8FF just below second BMP range (private use)
    h.assert_false(
      _IRIChars.is_ucschar(0xF8FF), "U+F8FF should not be ucschar")
    // U+F900 start of second BMP range
    h.assert_true(
      _IRIChars.is_ucschar(0xF900), "U+F900 should be ucschar")
    // U+FDCF end of second BMP range
    h.assert_true(
      _IRIChars.is_ucschar(0xFDCF), "U+FDCF should be ucschar")
    // U+FDD0 just past second BMP range (noncharacter)
    h.assert_false(
      _IRIChars.is_ucschar(0xFDD0), "U+FDD0 should not be ucschar")

    // U+FDEF just below third BMP range
    h.assert_false(
      _IRIChars.is_ucschar(0xFDEF), "U+FDEF should not be ucschar")
    // U+FDF0 start of third BMP range
    h.assert_true(
      _IRIChars.is_ucschar(0xFDF0), "U+FDF0 should be ucschar")
    // U+FFEF end of third BMP range
    h.assert_true(
      _IRIChars.is_ucschar(0xFFEF), "U+FFEF should be ucschar")
    // U+FFF0 just past third BMP range
    h.assert_false(
      _IRIChars.is_ucschar(0xFFF0), "U+FFF0 should not be ucschar")

    // Planes 1-13 boundaries
    // U+10000 start of plane 1
    h.assert_true(
      _IRIChars.is_ucschar(0x10000), "U+10000 should be ucschar")
    // U+1FFFD end of plane 1
    h.assert_true(
      _IRIChars.is_ucschar(0x1FFFD), "U+1FFFD should be ucschar")
    // U+1FFFE noncharacter in plane 1
    h.assert_false(
      _IRIChars.is_ucschar(0x1FFFE), "U+1FFFE should not be ucschar")
    // U+1FFFF noncharacter in plane 1
    h.assert_false(
      _IRIChars.is_ucschar(0x1FFFF), "U+1FFFF should not be ucschar")
    // U+DFFFD end of plane 13
    h.assert_true(
      _IRIChars.is_ucschar(0xDFFFD), "U+DFFFD should be ucschar")
    // U+DFFFE noncharacter in plane 13
    h.assert_false(
      _IRIChars.is_ucschar(0xDFFFE), "U+DFFFE should not be ucschar")

    // Plane 14 partial range
    // U+E0FFF just below E1000
    h.assert_false(
      _IRIChars.is_ucschar(0xE0FFF), "U+E0FFF should not be ucschar")
    // U+E1000 start
    h.assert_true(
      _IRIChars.is_ucschar(0xE1000), "U+E1000 should be ucschar")
    // U+EFFFD end
    h.assert_true(
      _IRIChars.is_ucschar(0xEFFFD), "U+EFFFD should be ucschar")
    // U+EFFFE just past
    h.assert_false(
      _IRIChars.is_ucschar(0xEFFFE), "U+EFFFE should not be ucschar")

    // -- iprivate boundaries --
    // U+DFFF just below private use
    h.assert_false(
      _IRIChars.is_iprivate(0xDFFF), "U+DFFF should not be iprivate")
    // U+E000 start of BMP private use
    h.assert_true(
      _IRIChars.is_iprivate(0xE000), "U+E000 should be iprivate")
    // U+F8FF end of BMP private use
    h.assert_true(
      _IRIChars.is_iprivate(0xF8FF), "U+F8FF should be iprivate")
    // U+F900 just past BMP private use
    h.assert_false(
      _IRIChars.is_iprivate(0xF900), "U+F900 should not be iprivate")

    // Plane 15
    h.assert_true(
      _IRIChars.is_iprivate(0xF0000), "U+F0000 should be iprivate")
    h.assert_true(
      _IRIChars.is_iprivate(0xFFFFD), "U+FFFFD should be iprivate")
    h.assert_false(
      _IRIChars.is_iprivate(0xFFFFE), "U+FFFFE should not be iprivate")

    // Plane 16
    h.assert_true(
      _IRIChars.is_iprivate(0x100000), "U+100000 should be iprivate")
    h.assert_true(
      _IRIChars.is_iprivate(0x10FFFD), "U+10FFFD should be iprivate")
    h.assert_false(
      _IRIChars.is_iprivate(0x10FFFE), "U+10FFFE should not be iprivate")

    // -- bidi format characters --
    h.assert_true(
      _IRIChars.is_bidi_format(0x200E), "U+200E should be bidi format")
    h.assert_true(
      _IRIChars.is_bidi_format(0x200F), "U+200F should be bidi format")
    h.assert_true(
      _IRIChars.is_bidi_format(0x202A), "U+202A should be bidi format")
    h.assert_true(
      _IRIChars.is_bidi_format(0x202E), "U+202E should be bidi format")
    // Neighbors are not bidi format
    h.assert_false(
      _IRIChars.is_bidi_format(0x200D),
      "U+200D should not be bidi format")
    h.assert_false(
      _IRIChars.is_bidi_format(0x2010),
      "U+2010 should not be bidi format")
    h.assert_false(
      _IRIChars.is_bidi_format(0x2029),
      "U+2029 should not be bidi format")
    h.assert_false(
      _IRIChars.is_bidi_format(0x202F),
      "U+202F should not be bidi format")

    // -- bidi chars are ucschar but should not be decoded --
    h.assert_true(
      _IRIChars.is_ucschar(0x200E), "U+200E is in ucschar range")
    h.assert_true(
      _IRIChars.is_ucschar(0x200F), "U+200F is in ucschar range")

class \nodoc\ iso _TestIRIToURIKnownGood is UnitTest
  """
  Known-good IRIToURI conversions.
  """
  fun name(): String => "uri/iri_to_uri/known_good"

  fun ref apply(h: TestHelper) =>
    // Pure ASCII unchanged
    _assert_iri_to_uri(
      h,
      "http://example.com/path?q=1#frag",
      "http://example.com/path?q=1#frag")

    // BMP ucschar encoded: é = U+00E9, UTF-8 bytes C3 A9
    _assert_iri_to_uri(
      h,
      "http://example.com/r\xE9sum\xE9",
      "http://example.com/r%C3%A9sum%C3%A9")

    // Supplementary plane character: U+1F600, UTF-8 bytes F0 9F 98 80
    _assert_iri_to_uri(
      h,
      "http://example.com/\U01F600",
      "http://example.com/%F0%9F%98%80")

    // Existing %XX triplets preserved
    _assert_iri_to_uri(
      h,
      "http://example.com/%20path",
      "http://example.com/%20path")

    // Mixed: literal non-ASCII + existing percent-encoding
    _assert_iri_to_uri(
      h,
      "http://example.com/caf\xE9/%20menu",
      "http://example.com/caf%C3%A9/%20menu")

    // Non-ASCII in host
    _assert_iri_to_uri(
      h,
      "http://\xE9xample.com/",
      "http://%C3%A9xample.com/")

    // Non-ASCII in query
    _assert_iri_to_uri(
      h,
      "http://example.com/?q=\xE9",
      "http://example.com/?q=%C3%A9")

    // Non-ASCII in fragment
    _assert_iri_to_uri(
      h,
      "http://example.com/#\xE9",
      "http://example.com/#%C3%A9")

  fun _assert_iri_to_uri(
    h: TestHelper,
    input: String,
    expected: String)
  =>
    match \exhaustive\ ParseURI(input)
    | let iri: URI val =>
      let uri = IRIToURI(iri)
      h.assert_eq[String val](
        expected, uri.string(), "IRIToURI(" + input + ")")
    | let e: URIParseError val =>
      h.fail("parse failed for " + input + ": " + e.string())
    end

class \nodoc\ iso _TestURIToIRIKnownGood is UnitTest
  """
  Known-good URIToIRI conversions.
  """
  fun name(): String => "uri/uri_to_iri/known_good"

  fun ref apply(h: TestHelper) =>
    // ucschar decoded: é = U+00E9 = %C3%A9
    _assert_uri_to_iri(
      h,
      "http://example.com/r%C3%A9sum%C3%A9",
      "http://example.com/r\xE9sum\xE9")

    // iprivate decoded in query only: U+E000 = %EE%80%80
    _assert_uri_to_iri(
      h,
      "http://example.com/path?q=%EE%80%80",
      "http://example.com/path?q=\uE000")

    // iprivate NOT decoded in path
    _assert_uri_to_iri(
      h,
      "http://example.com/%EE%80%80",
      "http://example.com/%EE%80%80")

    // iprivate NOT decoded in fragment
    _assert_uri_to_iri(
      h,
      "http://example.com/#%EE%80%80",
      "http://example.com/#%EE%80%80")

    // Bidi chars stay encoded: U+200E = %E2%80%8E
    _assert_uri_to_iri(
      h,
      "http://example.com/%E2%80%8E",
      "http://example.com/%E2%80%8E")

    // Bidi chars stay encoded even in query: U+200F = %E2%80%8F
    _assert_uri_to_iri(
      h,
      "http://example.com/?q=%E2%80%8F",
      "http://example.com/?q=%E2%80%8F")

    // Invalid UTF-8 stays encoded (0xFF is not a valid leading byte)
    _assert_uri_to_iri(
      h,
      "http://example.com/%FF",
      "http://example.com/%FF")

    // Incomplete UTF-8 sequence stays encoded (C3 without continuation)
    _assert_uri_to_iri(
      h,
      "http://example.com/%C3",
      "http://example.com/%C3")

    // ASCII percent-encoding stays as-is
    _assert_uri_to_iri(
      h,
      "http://example.com/%20path",
      "http://example.com/%20path")

    // Pure ASCII unchanged
    _assert_uri_to_iri(
      h,
      "http://example.com/path",
      "http://example.com/path")

    // Supplementary plane ucschar decoded: U+10000 = %F0%90%80%80
    _assert_uri_to_iri(
      h,
      "http://example.com/%F0%90%80%80",
      "http://example.com/\U010000")

    // Lowercase hex digits handled
    _assert_uri_to_iri(
      h,
      "http://example.com/r%c3%a9sum%c3%a9",
      "http://example.com/r\xE9sum\xE9")

    // Non-ucschar, non-iprivate non-ASCII stays encoded
    // U+009F = C2 9F — below ucschar range
    _assert_uri_to_iri(
      h,
      "http://example.com/%C2%9F",
      "http://example.com/%C2%9F")

    // Malformed percent-encoding (%GG) passes through unchanged
    _assert_uri_to_iri(
      h,
      "http://example.com/%GG",
      "http://example.com/%GG")

  fun _assert_uri_to_iri(
    h: TestHelper,
    input: String,
    expected: String)
  =>
    match \exhaustive\ ParseURI(input)
    | let uri: URI val =>
      let iri = URIToIRI(uri)
      h.assert_eq[String val](
        expected, iri.string(), "URIToIRI(" + input + ")")
    | let e: URIParseError val =>
      h.fail("parse failed for " + input + ": " + e.string())
    end

class \nodoc\ iso _TestNormalizeIRIKnownGood is UnitTest
  """
  Known-good NormalizeIRI and IRIEquivalent examples.
  """
  fun name(): String => "uri/normalize_iri/known_good"

  fun ref apply(h: TestHelper) =>
    // Mixed-case scheme + encoded ucschar normalized
    _assert_normalize_iri(
      h,
      "HTTP://Example.COM/r%C3%A9sum%C3%A9",
      "http://example.com/r\xE9sum\xE9")

    // Default port removal + ucschar decoding
    _assert_normalize_iri(
      h,
      "http://example.com:80/caf%C3%A9",
      "http://example.com/caf\xE9")

    // Dot segment removal + ucschar decoding
    _assert_normalize_iri(
      h,
      "http://example.com/a/../%C3%A9",
      "http://example.com/\xE9")

    // Encoded unreserved decoded + ucschar decoded
    _assert_normalize_iri(
      h,
      "http://example.com/%7E%C3%A9",
      "http://example.com/~\xE9")

    // IRI/URI cross-equivalence
    _assert_iri_equivalent(
      h,
      "http://example.com/r\xE9sum\xE9",
      "http://example.com/r%C3%A9sum%C3%A9",
      true)

    // IRI with case difference
    _assert_iri_equivalent(
      h,
      "HTTP://Example.COM/caf\xE9",
      "http://example.com/caf%c3%a9",
      true)

    // Different paths are not equivalent: è (U+00E8) vs é (U+00E9)
    _assert_iri_equivalent(
      h,
      "http://example.com/\xE9",
      "http://example.com/\xE8",
      false)

  fun _assert_normalize_iri(
    h: TestHelper,
    input: String,
    expected: String)
  =>
    match \exhaustive\ ParseURI(input)
    | let u: URI val =>
      match \exhaustive\ NormalizeIRI(u)
      | let normalized: URI val =>
        h.assert_eq[String val](
          expected,
          normalized.string(),
          "NormalizeIRI(" + input + ")")
      | let e: InvalidPercentEncoding val =>
        h.fail("normalization failed for " + input + ": " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse failed for " + input + ": " + e.string())
    end

  fun _assert_iri_equivalent(
    h: TestHelper,
    a_str: String,
    b_str: String,
    expected: Bool)
  =>
    match \exhaustive\ (ParseURI(a_str), ParseURI(b_str))
    | (let a: URI val, let b: URI val) =>
      match \exhaustive\ IRIEquivalent(a, b)
      | let result: Bool =>
        h.assert_eq[Bool](
          expected,
          result,
          "IRIEquivalent(" + a_str + ", " + b_str + ")")
      | let e: InvalidPercentEncoding val =>
        h.fail("equivalence failed for (" + a_str + ", " + b_str +
          "): " + e.string())
      end
    else
      h.fail("parse failed for (" + a_str + ", " + b_str + ")")
    end

// ============================================================================
// Generators
// ============================================================================
primitive \nodoc\ _IRIStringGenerator
  """
  Generate URI strings with mixed ASCII and Unicode content.
  """
  fun apply(): Generator[String val] =>
    Generators.one_of[String val](
      [
        // Pure ASCII
        "http://example.com/path?q=1#frag"
        // BMP ucschar: é (U+00E9)
        "http://example.com/caf\xE9"
        // Supplementary plane: U+1F600
        "http://example.com/\U01F600"
        // Mixed ASCII and ucschar
        "http://example.com/r\xE9sum\xE9?q=\xE9"
        // Non-ASCII in host: ü (U+00FC)
        "http://b\xFCcher.example.com/"
        // ucschar in fragment
        "http://example.com/path#\xE9"
        // Multiple non-ASCII chars: àèì
        "http://example.com/\xE0/\xE8/\xEC"
        // CJK character: 世 (U+4E16)
        "http://example.com/\u4E16"
      ])

primitive \nodoc\ _URIWithEncodedNonASCIIGenerator
  """
  Generate URI strings with percent-encoded non-ASCII bytes.
  """
  fun apply(): Generator[String val] =>
    Generators.one_of[String val](
      [
        // Encoded ucschar: é = C3 A9
        "http://example.com/r%C3%A9sum%C3%A9"
        // Encoded supplementary: U+10000 = F0 90 80 80
        "http://example.com/%F0%90%80%80"
        // Encoded iprivate in query: U+E000 = EE 80 80
        "http://example.com/?q=%EE%80%80"
        // Mixed ucschar and ASCII encoding
        "http://example.com/%C3%A9/%20"
        // Non-ucschar stays encoded: U+009F = C2 9F
        "http://example.com/%C2%9F"
        // Bidi format char: U+200E = E2 80 8E
        "http://example.com/%E2%80%8E"
        // Encoded CJK: U+4E16 = E4 B8 96
        "http://example.com/%E4%B8%96"
        // Lowercase hex
        "http://example.com/%c3%a9"
      ])

primitive \nodoc\ _IRIUcscharOnlyGenerator
  """
  Generate IRI strings containing only ucschar non-ASCII codepoints
  (no iprivate), suitable for roundtrip testing.
  """
  fun apply(): Generator[String val] =>
    Generators.one_of[String val](
      [
        // é in path
        "http://example.com/caf\xE9"
        // Multiple BMP ucschar: àè
        "http://example.com/\xE0/\xE8"
        // CJK in path: 世界
        "http://example.com/\u4E16\u754C"
        // ucschar in query
        "http://example.com/?q=\xE9"
        // ucschar in fragment
        "http://example.com/#\xE9"
        // Supplementary plane: U+10000
        "http://example.com/\U010000"
        // Pure ASCII (trivial case)
        "http://example.com/path"
      ])

primitive \nodoc\ _UcscharStringGenerator
  """
  Generate raw strings containing ucschar codepoints.
  """
  fun apply(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "caf\xE9"           // é
        "\xE0\xE8\xEC"      // àèì
        "\u4E16\u754C"       // 世界
        "\U010000"         // U+10000
        "r\xE9sum\xE9"      // résumé
        "hello"              // pure ASCII
      ])
