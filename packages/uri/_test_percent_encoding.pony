use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyPercentRoundtrip is Property1[String val]
  """
  PercentDecode(PercentEncode(s, part)) roundtrips for arbitrary strings.
  Tests with URIPartPath as representative; the encode/decode cycle should
  preserve content regardless of which characters are encoded.
  """
  fun name(): String => "uri/percent_encoding/roundtrip"

  fun gen(): Generator[String val] =>
    Generators.ascii(0, 100)

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let encoded = PercentEncode(arg1, URIPartPath)
    match \exhaustive\ PercentDecode(encoded)
    | let decoded: String val =>
      ph.assert_eq[String val](arg1, decoded)
    | let err: InvalidPercentEncoding val =>
      ph.fail("roundtrip decode failed for: " + arg1)
    end

class \nodoc\ iso _PropertyPercentEncodeOutputLegal
  is Property1[String val]
  """
  PercentEncode output for path contains only RFC 3986-legal characters
  for the path component: unreserved, sub-delims, ':', '@', '/', and
  percent-encoded sequences.
  """
  fun name(): String => "uri/percent_encoding/output_legal"

  fun gen(): Generator[String val] =>
    Generators.ascii(1, 100)

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let encoded = PercentEncode(arg1, URIPartPath)
    var i: USize = 0
    while i < encoded.size() do
      try
        let c = encoded(i)?
        if c == '%' then
          // Must be followed by exactly two hex digits
          ph.assert_true(
            (i + 2) < encoded.size(),
            "truncated percent encoding at position " + i.string())
          try
            let h1 = encoded(i + 1)?
            let h2 = encoded(i + 2)?
            ph.assert_true(
              _is_hex(h1),
              "non-hex digit after % at position " + (i + 1).string())
            ph.assert_true(
              _is_hex(h2),
              "non-hex digit after % at position " + (i + 2).string())
          end
          i = i + 3
        else
          ph.assert_true(
            _is_path_legal(c),
            "illegal character in encoded output: " + String.from_array([c]) +
              " (0x" + _hex_string(c) + ")")
          i = i + 1
        end
      else
        ph.fail("index out of bounds at " + i.string())
        return
      end
    end

  fun _is_hex(c: U8): Bool =>
    ((c >= '0') and (c <= '9')) or
      ((c >= 'A') and (c <= 'F')) or
      ((c >= 'a') and (c <= 'f'))

  fun _is_path_legal(c: U8): Bool =>
    // unreserved
    ((c >= 'A') and (c <= 'Z')) or
      ((c >= 'a') and (c <= 'z')) or
      ((c >= '0') and (c <= '9')) or
      (c == '-') or (c == '.') or (c == '_') or (c == '~') or
      // sub-delims
      (c == '!') or (c == '$') or (c == '&') or (c == '\'') or
      (c == '(') or (c == ')') or (c == '*') or (c == '+') or
      (c == ',') or (c == ';') or (c == '=') or
      // path-specific
      (c == ':') or (c == '@') or (c == '/')

  fun _hex_string(c: U8): String val =>
    let hex = "0123456789ABCDEF"
    recover val
      let out = String(2)
      try
        out.push(hex(c.usize() >> 4)?)
        out.push(hex(c.usize() and 0x0F)?)
      end
      out
    end

class \nodoc\ iso _PropertyInvalidPercentSequenceRejected
  is Property1[String val]
  """
  Invalid percent sequences — truncated, non-hex digits — produce
  InvalidPercentEncoding.
  """
  fun name(): String => "uri/percent_encoding/invalid_rejected"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        // trailing percent with no hex digits
        (1, Generators.ascii(0, 10)
          .map[String val]({(s) => s + "%" }))
        // trailing percent with only one hex digit
        (1, Generators.ascii(0, 10)
          .map[String val]({(s) => s + "%A" }))
        // non-hex after percent
        (1, Generators.ascii(0, 10)
          .map[String val]({(s) => s + "%GG" }))
        // non-hex in second position
        (1, Generators.ascii(0, 10)
          .map[String val]({(s) => s + "%AZ" }))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ PercentDecode(arg1)
    | let s: String val =>
      ph.fail("should have rejected: " + arg1)
    | let err: InvalidPercentEncoding val =>
      ph.assert_true(true)
    end

class \nodoc\ iso _PropertyPercentDecodeBoundary
  is Property1[(String val, Bool)]
  """
  Mixed valid/invalid generator — PercentDecode succeeds iff input is the
  valid variant.
  """
  fun name(): String => "uri/percent_encoding/decode_boundary"

  fun gen(): Generator[(String val, Bool)] =>
    let valid_gen: Generator[(String val, Bool)] =
      Generators.ascii(0, 50)
        .map[String val]({(s) => PercentEncode(s, URIPartPath) })
        .map[(String val, Bool)]({(s) => (s, true) })

    let invalid_gen: Generator[(String val, Bool)] =
      Generators.frequency[String val](
        [
          as WeightedGenerator[String val]:
          (1, Generators.ascii(0, 10)
            .map[String val]({(s) => s + "%" }))
          (1, Generators.ascii(0, 10)
            .map[String val]({(s) => s + "%A" }))
          (1, Generators.ascii(0, 10)
            .map[String val]({(s) => s + "%GG" }))
          (1, Generators.ascii(0, 10)
            .map[String val]({(s) => s + "%AZ" }))
        ]).map[(String val, Bool)]({(s) => (s, false) })

    Generators.frequency[(String val, Bool)](
      [
        as WeightedGenerator[(String val, Bool)]:
        (1, valid_gen)
        (1, invalid_gen)
      ])

  fun ref property(arg1: (String val, Bool), ph: PropertyHelper) =>
    (let input, let should_succeed) = arg1
    let result = PercentDecode(input)
    if should_succeed then
      match \exhaustive\ result
      | let s: String val =>
        ph.assert_true(true)
      | let err: InvalidPercentEncoding val =>
        ph.fail("expected decode success for: " + input)
      end
    else
      match \exhaustive\ result
      | let s: String val =>
        ph.fail("expected decode failure for: " + input)
      | let err: InvalidPercentEncoding val =>
        ph.assert_true(true)
      end
    end

class \nodoc\ iso _TestPercentEncodeKnownGood is UnitTest
  """
  Known encodings from RFC 3986 section 2.1 and common cases.
  """
  fun name(): String => "uri/percent_encoding/known_good"

  fun ref apply(h: TestHelper) =>
    // Space encodes as %20
    h.assert_eq[String val]("%20", PercentEncode(" ", URIPartPath))

    // Unreserved characters pass through unchanged
    h.assert_eq[String val](
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~",
      PercentEncode(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~",
        URIPartPath))

    // RFC 3986 section 2.1 example: 'A' = %41
    match \exhaustive\ PercentDecode("%41")
    | let s: String val => h.assert_eq[String val]("A", s)
    | let err: InvalidPercentEncoding val =>
      h.fail("should decode %41")
    end

    // Lowercase hex is accepted
    match \exhaustive\ PercentDecode("%6a")
    | let s: String val => h.assert_eq[String val]("j", s)
    | let err: InvalidPercentEncoding val =>
      h.fail("should decode %6a")
    end

    // Empty string roundtrips
    h.assert_eq[String val]("", PercentEncode("", URIPartPath))
    match \exhaustive\ PercentDecode("")
    | let s: String val => h.assert_eq[String val]("", s)
    | let err: InvalidPercentEncoding val =>
      h.fail("should decode empty string")
    end

    // PercentEncode uses uppercase hex
    h.assert_eq[String val]("%3C", PercentEncode("<", URIPartPath))

    // Path allows / and : unencoded
    h.assert_eq[String val]("/foo:bar", PercentEncode("/foo:bar", URIPartPath))

    // Query allows / : ? unencoded
    h.assert_eq[String val](
      "/foo:bar?baz", PercentEncode("/foo:bar?baz", URIPartQuery))

    // Userinfo allows : but encodes @
    h.assert_eq[String val](
      "user:pass", PercentEncode("user:pass", URIPartUserinfo))
    h.assert_eq[String val](
      "user%40host", PercentEncode("user@host", URIPartUserinfo))
