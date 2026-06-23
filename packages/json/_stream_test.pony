use "pony_test"
use "pony_check"

primitive \nodoc\ _StreamHelp
  """Test helpers for driving JsonStreamParser."""
  fun drain(p: JsonStreamParser, out: Array[JsonValue]): (None | JsonParseError)
  =>
    var go = true
    var err: (None | JsonParseError) = None
    while go do
      match p.pull()
      | let v: JsonValue => out.push(v)
      | JsonNeedMore => go = false
      | let e: JsonParseError => err = e; go = false
      end
    end
    err

  fun all(input: String): (Array[JsonValue] | JsonParseError) =>
    """Feed `input` as a single chunk and collect all complete values."""
    let p = JsonStreamParser
    p.feed(input)
    let out = Array[JsonValue]
    match drain(p, out)
    | let e: JsonParseError => e
    else out
    end

  fun split(input: String, n: USize): (Array[JsonValue] | JsonParseError) =>
    """Feed `input` in chunks of `n` bytes, collecting values as they complete."""
    let p = JsonStreamParser
    let out = Array[JsonValue]
    var i: USize = 0
    while i < input.size() do
      let upper = (i + n).min(input.size())
      p.feed(input.trim(i, upper))
      match drain(p, out)
      | let e: JsonParseError => return e
      end
      i = upper
    end
    out

  fun render(values: Array[JsonValue] box): String =>
    """Canonical text of a value list, for structural equality comparison."""
    let s = String
    for v in values.values() do
      s.append(JsonPrinter.print(v))
      s.push('\n')
    end
    s.clone()

class \nodoc\ iso _TestStreamObject is UnitTest
  fun name(): String => "json/stream/object"

  fun apply(h: TestHelper) ? =>
    match _StreamHelp.all("""{"a":1,"b":true,"c":null,"d":"hi"}""")
    | let vs: Array[JsonValue] =>
      h.assert_eq[USize](1, vs.size())
      let nav = JsonNav(vs(0)?)
      h.assert_eq[I64](1, nav("a").as_i64()?)
      h.assert_eq[Bool](true, nav("b").as_bool()?)
      nav("c").as_null()? // a JSON null (raises if not null)
      h.assert_eq[String]("hi", nav("d").as_string()?)
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestStreamArray is UnitTest
  fun name(): String => "json/stream/array"

  fun apply(h: TestHelper) ? =>
    match _StreamHelp.all("[1,2,3]")
    | let vs: Array[JsonValue] =>
      h.assert_eq[USize](1, vs.size())
      let arr = JsonNav(vs(0)?).as_array()?
      h.assert_eq[USize](3, arr.size())
      h.assert_eq[I64](2, JsonNav(arr(1)?).as_i64()?)
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestStreamEmptyContainers is UnitTest
  fun name(): String => "json/stream/empty-containers"

  fun apply(h: TestHelper) ? =>
    match _StreamHelp.all("""{} [] [[]] {"a":{}}""")
    | let vs: Array[JsonValue] =>
      h.assert_eq[USize](4, vs.size())
      h.assert_eq[USize](0, JsonNav(vs(0)?).as_object()?.size())
      h.assert_eq[USize](0, JsonNav(vs(1)?).as_array()?.size())
      h.assert_eq[USize](1, JsonNav(vs(2)?).as_array()?.size())
      h.assert_eq[USize](0, JsonNav(vs(3)?)("a").as_object()?.size())
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestStreamMultiValue is UnitTest
  fun name(): String => "json/stream/multi-value"

  fun apply(h: TestHelper) ? =>
    match _StreamHelp.all("""{"a":1} {"b":2} [3]""")
    | let vs: Array[JsonValue] =>
      h.assert_eq[USize](3, vs.size())
      h.assert_eq[I64](1, JsonNav(vs(0)?)("a").as_i64()?)
      h.assert_eq[I64](2, JsonNav(vs(1)?)("b").as_i64()?)
      h.assert_eq[I64](3, JsonNav(vs(2)?)(USize(0)).as_i64()?)
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestStreamNeedMore is UnitTest
  fun name(): String => "json/stream/need-more"

  fun apply(h: TestHelper) ? =>
    let p = JsonStreamParser
    p.feed("""{"a":""")
    match p.pull()
    | JsonNeedMore => None
    else h.fail("expected JsonNeedMore on incomplete value")
    end
    p.feed("1}")
    match p.pull()
    | let v: JsonValue => h.assert_eq[I64](1, JsonNav(v)("a").as_i64()?)
    else h.fail("expected a value after the rest arrived")
    end

class \nodoc\ iso _TestStreamSplitInvariance is UnitTest
  """The token stream must be identical regardless of where chunks are split."""
  fun name(): String => "json/stream/split-invariance"

  fun apply(h: TestHelper) =>
    let input =
      """{"name":"alice","tags":["x","y"],"n":-12.5e3,"nested":{"deep":[true,null]},"u":"sm😀ile\n\t"} [1,2,{"k":false}]"""
    let whole =
      match _StreamHelp.all(input)
      | let vs: Array[JsonValue] => _StreamHelp.render(vs)
      | let e: JsonParseError => h.fail("whole parse failed: " + e.string()); ""
      end
    for n in [as USize: 1; 2; 3; 5; 7; 13].values() do
      match _StreamHelp.split(input, n)
      | let vs: Array[JsonValue] =>
        h.assert_eq[String](whole, _StreamHelp.render(vs)
          where msg = "mismatch at chunk size " + n.string())
      | let e: JsonParseError =>
        h.fail("split parse failed at chunk size " + n.string()
          + ": " + e.string())
      end
    end

class \nodoc\ iso _TestStreamEscapes is UnitTest
  fun name(): String => "json/stream/escapes"

  fun apply(h: TestHelper) ? =>
    // Every C-style escape, a BMP \uXXXX escape, and a \uXXXX surrogate pair —
    // decoded the same as the batch parser. (The triple-quoted literal is not
    // escape-processed by Pony, so \", \uD83D, etc. reach the parser verbatim.)
    let input = """{"s":"q\"b\\s\/f\b\f\n\r\t\u00E9-\uD83D\uDE00"}"""
    let expected =
      match JsonParser.parse(input)
      | let v: JsonValue => JsonNav(v)("s").as_string()?
      | let e: JsonParseError => h.fail("batch failed: " + e.string()); ""
      end
    // Split at size 1 forces the scanner to suspend mid-escape, mid-\uXXXX hex,
    // and between the two \u of the surrogate pair.
    match _StreamHelp.split(input, 1)
    | let vs: Array[JsonValue] =>
      h.assert_eq[String](expected, JsonNav(vs(0)?)("s").as_string()?)
    | let e: JsonParseError => h.fail("stream failed: " + e.string())
    end

    // Invalid/lone surrogates are rejected by the streaming parser too.
    for bad in
      [ """["\uD800"]"""; """["\uDC00"]"""; """["\uD800A"]""" ].values()
    do
      match _StreamHelp.all(bad)
      | let v2: Array[JsonValue] => h.fail("expected a surrogate error: " + bad)
      | let e2: JsonParseError => None
      end
    end

class \nodoc\ iso _TestStreamNumbers is UnitTest
  fun name(): String => "json/stream/numbers"

  fun apply(h: TestHelper) ? =>
    match _StreamHelp.all("[0,-1,42,3.14,-2.5e-3,1E10,123456789012345678901]")
    | let vs: Array[JsonValue] =>
      let a = JsonNav(vs(0)?).as_array()?
      h.assert_eq[I64](0, JsonNav(a(0)?).as_i64()?)
      h.assert_eq[I64](-1, JsonNav(a(1)?).as_i64()?)
      h.assert_eq[I64](42, JsonNav(a(2)?).as_i64()?)
      h.assert_eq[F64](3.14, JsonNav(a(3)?).as_f64()?)
      h.assert_eq[F64](-2.5e-3, JsonNav(a(4)?).as_f64()?) // negative exponent
      h.assert_eq[F64](1e10, JsonNav(a(5)?).as_f64()?)    // uppercase E
      // >18 digit integer is forced to F64
      h.assert_true(JsonNav(a(6)?).as_f64()? > 1.0e20)
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestStreamNumberForms is UnitTest
  """Streaming numbers decode to the exact expected value, and rejects hold."""
  fun name(): String => "json/stream/number-forms"

  fun apply(h: TestHelper) ? =>
    // Assert the exact decoded value (not a printed form), so a divergence
    // below printer precision cannot hide.
    h.assert_eq[I64](0, _i64(h, "[0]")?)
    h.assert_eq[I64](0, _i64(h, "[-0]")?)
    // 18 integer digits stay I64 (max 18-digit < I64.max)
    h.assert_eq[I64](123456789012345678, _i64(h, "[123456789012345678]")?)
    h.assert_eq[F64](0.0, _f64(h, "[0e0]")?)
    h.assert_eq[F64](1e10, _f64(h, "[1E10]")?)   // uppercase E
    h.assert_eq[F64](-2.5e-3, _f64(h, "[-2.5e-3]")?) // signed exponent
    // 19 digits force F64 — assert stream agrees with the batch parse
    _agree(h, "[1234567890123456789]")

    // malformed numbers (the over-consume-then-validate path) must be rejected
    for bad in
      [ "[1-2]"; "[1.2.3]"; "[1e]"; "[.5]"; "[+5]"; "[--1]"; "[1.]"; "[01]"
        "[1e+e1]" ].values()
    do
      match _StreamHelp.all(bad)
      | let vs: Array[JsonValue] => h.fail("expected a number error: " + bad)
      | let e: JsonParseError => None
      end
    end

  fun _i64(h: TestHelper, doc: String): I64 ? =>
    match _StreamHelp.all(doc)
    | let vs: Array[JsonValue] => JsonNav(vs(0)?)(USize(0)).as_i64()?
    | let e: JsonParseError => h.fail(doc + ": " + e.string()); error
    end

  fun _f64(h: TestHelper, doc: String): F64 ? =>
    match _StreamHelp.all(doc)
    | let vs: Array[JsonValue] => JsonNav(vs(0)?)(USize(0)).as_f64()?
    | let e: JsonParseError => h.fail(doc + ": " + e.string()); error
    end

  fun _agree(h: TestHelper, doc: String) =>
    let batch =
      match JsonParser.parse(doc)
      | let v: JsonValue => JsonPrinter.print(v)
      | let e: JsonParseError => h.fail("batch failed on " + doc); ""
      end
    match _StreamHelp.all(doc)
    | let vs: Array[JsonValue] =>
      if vs.size() == 1 then
        try h.assert_eq[String](batch, JsonPrinter.print(vs(0)?)) end
      else
        h.fail("expected exactly one value for " + doc)
      end
    | let e: JsonParseError => h.fail("stream failed on " + doc)
    end

class \nodoc\ iso _TestStreamTrailingComma is UnitTest
  fun name(): String => "json/stream/trailing-comma"

  fun apply(h: TestHelper) =>
    _expect_error(h, "[1,]")
    _expect_error(h, """{"a":1,}""")
    _expect_error(h, "[1,2,]")

  fun _expect_error(h: TestHelper, input: String) =>
    match _StreamHelp.all(input)
    | let vs: Array[JsonValue] =>
      h.fail("expected error for: " + input)
    | let e: JsonParseError => None
    end

class \nodoc\ iso _TestStreamMalformed is UnitTest
  fun name(): String => "json/stream/malformed"

  fun apply(h: TestHelper) =>
    for bad in
      [ """{"a":}"""; """{,}"""; """{"a" 1}"""; """[1 2]"""; """{"a":1 "b":2}"""
        """[treu]"""; """[nul]"""; """[1,,2]"""; """{"a":1]""" ].values()
    do
      match _StreamHelp.all(bad)
      | let vs: Array[JsonValue] => h.fail("expected error for: " + bad)
      | let e: JsonParseError => None
      end
    end

class \nodoc\ iso _TestStreamScalarRootRejected is UnitTest
  """Top-level values must be containers; bare scalars are out of scope."""
  fun name(): String => "json/stream/scalar-root-rejected"

  fun apply(h: TestHelper) =>
    for bad in ["42"; """"hi"""" ; "true"; "null"; "  3.14  "].values() do
      match _StreamHelp.all(bad)
      | let vs: Array[JsonValue] => h.fail("expected error for scalar: " + bad)
      | let e: JsonParseError => None
      end
    end

class \nodoc\ iso _TestStreamErrorLatches is UnitTest
  fun name(): String => "json/stream/error-latches"

  fun apply(h: TestHelper) =>
    let p = JsonStreamParser
    p.feed("""{"a":}""")
    match p.pull()
    | let e: JsonParseError => None
    else h.fail("expected an error")
    end
    // latched: further pulls keep returning the error, even after feeding more
    p.feed("""{"b":2}""")
    match p.pull()
    | let e: JsonParseError => None
    else h.fail("error did not latch")
    end

class \nodoc\ iso _TestStreamLimitDepth is UnitTest
  fun name(): String => "json/stream/limit-depth"

  fun apply(h: TestHelper) =>
    let limits = JsonStreamLimits(where max_depth' = 3)
    let p = JsonStreamParser(limits)
    p.feed("[[[[1]]]]") // depth 4 > 3
    match p.pull()
    | let e: JsonParseError => None
    else h.fail("expected a depth-limit error")
    end
    // depth exactly at the limit is fine
    match _StreamHelp.all("[[[1]]]")
    | let vs: Array[JsonValue] => h.assert_eq[USize](1, vs.size())
    | let e: JsonParseError => h.fail("depth 3 should be allowed: " + e.string())
    end

class \nodoc\ iso _TestStreamLimits is UnitTest
  fun name(): String => "json/stream/limits"

  fun apply(h: TestHelper) =>
    // max_string_len (tight: a string of exactly the limit is accepted, +1 not)
    _accept(h, JsonStreamLimits(where max_string_len' = 4), """["abcd"]""")
    _reject(h, JsonStreamLimits(where max_string_len' = 4), """["abcde"]""")
    // max_number_len (tight: a number of exactly the limit is accepted, +1 not)
    _accept(h, JsonStreamLimits(where max_number_len' = 5), "[12345]")
    _reject(h, JsonStreamLimits(where max_number_len' = 5), "[123456]")
    // max_value_bytes (whole value is 7 bytes)
    _reject(h, JsonStreamLimits(where max_value_bytes' = 4), "[1,2,3]")
    _accept(h, JsonStreamLimits(where max_value_bytes' = 7), "[1,2,3]")
    // a raised max_depth lifts the default bound: 9000-deep nesting exceeds the
    // default max_depth (1024), so the default rejects it while max_depth'=10000
    // accepts it.
    let s = String
    var i: USize = 0
    while i < 9000 do s.push('['); i = i + 1 end
    i = 0
    while i < 9000 do s.push(']'); i = i + 1 end
    let deep: String = s.clone()
    _reject(h, JsonStreamLimits, deep)
    _accept(h, JsonStreamLimits(where max_depth' = 10000), deep)

    // whitespace before/between values must not count toward the value budget
    let p = JsonStreamParser(JsonStreamLimits(where max_value_bytes' = 3))
    p.feed("   [1]      [2]") // each value 3 bytes; spaces around/between
    let out = Array[JsonValue]
    match _StreamHelp.drain(p, out)
    | let e: JsonParseError =>
      h.fail("whitespace counted against the value budget: " + e.string())
    end
    h.assert_eq[USize](2, out.size())

    // a limit is enforced even when the value is split across chunks, with no
    // single chunk exceeding the cap
    let cp = JsonStreamParser(JsonStreamLimits(where max_string_len' = 6))
    cp.feed("[\"abc")  // 3 string bytes so far
    cp.feed("defg\"]") // "abcdefg" is 7 > 6
    match _StreamHelp.drain(cp, Array[JsonValue])
    | None => h.fail("max_string_len not enforced across a chunk boundary")
    end

  fun _reject(h: TestHelper, limits: JsonStreamLimits, input: String) =>
    let p = JsonStreamParser(limits)
    p.feed(input)
    match _StreamHelp.drain(p, Array[JsonValue])
    | None => h.fail("expected a limit error for: " + input)
    end

  fun _accept(h: TestHelper, limits: JsonStreamLimits, input: String) =>
    let p = JsonStreamParser(limits)
    p.feed(input)
    let out = Array[JsonValue]
    match _StreamHelp.drain(p, out)
    | let e: JsonParseError =>
      h.fail("unexpected error for " + input + ": " + e.string())
    end
    h.assert_true(out.size() >= 1, "expected a value for: " + input)

class \nodoc\ iso _TestStreamDefaultDepthPrintable is UnitTest
  """
  A value nested at the default max_depth must survive the recursive consumers.

  The parser is iterative (an explicit frame stack), so it never overflows the
  native stack no matter how deep the input. But `JsonPrinter` and `JsonNav`
  recurse over the assembled tree, and the native stack is finite. The default
  `max_depth` exists to keep a streamed value safe to hand to those APIs, so
  this round-trips a doc nested exactly at the default cap through the parser
  and back out through the compact printer (which recurses the full depth). If
  a platform's scheduler-thread stack can't take the default depth, this is the
  test that fails — loudly — rather than a user's program crashing in the field.
  """
  fun name(): String => "json/stream/default_depth_printable"

  fun apply(h: TestHelper) =>
    let limits: JsonStreamLimits = JsonStreamLimits
    let depth = limits.max_depth
    let doc = String((depth * 2) + 1)
    var i: USize = 0
    while i < depth do doc.push('['); i = i + 1 end
    i = 0
    while i < depth do doc.push(']'); i = i + 1 end
    let input: String val = doc.clone()

    let p = JsonStreamParser // default limits
    p.feed(input)
    match p.pull()
    | let v: JsonValue =>
      // A deep array of empty arrays prints back to its exact source, so an
      // exact round-trip confirms both the parse and the recursive descent
      // survived the full depth.
      h.assert_eq[String val](input, JsonPrinter.print(v))
    | JsonNeedMore =>
      h.fail("a doc at the default max_depth should parse, got JsonNeedMore")
    | let e: JsonParseError =>
      h.fail("a doc at the default max_depth errored: " + e.string())
    end

class \nodoc\ iso _TestStreamProtocol is UnitTest
  """feed/pull lifecycle edges."""
  fun name(): String => "json/stream/protocol"

  fun apply(h: TestHelper) ? =>
    // pull() before any feed() is JsonNeedMore, not an error
    match JsonStreamParser.pull()
    | JsonNeedMore => None
    else h.fail("pull() before feed() should be JsonNeedMore")
    end

    // empty feeds are harmless no-ops; a real chunk still parses
    let q = JsonStreamParser
    q.feed("")
    q.feed("[1]")
    q.feed("")
    match q.pull()
    | let v: JsonValue => h.assert_eq[USize](1, JsonNav(v).as_array()?.size())
    else h.fail("empty feeds should not disturb parsing")
    end

    // a complete value followed by garbage: the value is returned, then the
    // next pull() latches an error (the design's named example)
    let p = JsonStreamParser
    p.feed("[1,2,3] x")
    match p.pull()
    | let v: JsonValue => h.assert_eq[USize](3, JsonNav(v).as_array()?.size())
    else h.fail("expected the leading value")
    end
    match p.pull()
    | let e: JsonParseError => None
    else h.fail("expected an error on the trailing garbage")
    end

class \nodoc\ iso _TestStreamErrorLocation is UnitTest
  """A parse error reports byte offset and line absolute across all fed chunks."""
  fun name(): String => "json/stream/error-location"

  fun apply(h: TestHelper) =>
    // The malformed byte ('9' where ':' is expected) is in the SECOND chunk,
    // on the second line. The reported offset must be at least the first
    // chunk's length — proving offsets accumulate across chunks, not reset.
    let first = "{\n  \"a\" "
    let p = JsonStreamParser
    p.feed(first)
    p.feed("9}")
    match p.pull()
    | let e: JsonParseError =>
      h.assert_true(e.offset >= first.size(),
        "error offset " + e.offset.string() + " is not absolute across chunks")
      h.assert_eq[USize](2, e.line)
    else
      h.fail("expected a parse error")
    end

class \nodoc\ iso _TestStreamDifferential is UnitTest
  """Streaming and batch parsing agree on the resulting value."""
  fun name(): String => "json/stream/differential"

  fun apply(h: TestHelper) =>
    let docs =
      [ """{"a":1,"b":[2,3.5,-4],"c":{"d":"e","f":null,"g":true}}"""
        """[{"x":1},{"y":[]},{"z":{}}]"""
        """{"unicode":"café 😀","esc":"a\tb\nc"}""" ]
    for doc in docs.values() do
      let batch =
        match JsonParser.parse(doc)
        | let v: JsonValue => JsonPrinter.print(v)
        | let e: JsonParseError =>
          h.fail("batch failed on " + doc + ": " + e.string()); ""
        end
      match _StreamHelp.all(doc)
      | let vs: Array[JsonValue] =>
        if vs.size() == 1 then
          try h.assert_eq[String](batch, JsonPrinter.print(vs(0)?)) end
        else
          h.fail("expected exactly one value for " + doc)
        end
      | let e: JsonParseError =>
        h.fail("stream failed on " + doc + ": " + e.string())
      end
    end

// ===================================================================
// Property tests
// ===================================================================

primitive \nodoc\ _JsonDocStringGen
  """
  Generates a JSON document whose root is an object or array — the form the
  streaming parser accepts — reusing the package's value-string generator.
  """
  fun apply(max_depth: USize = 3): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String =>
          if rnd.bool() then
            _JsonValueStringGen._gen_array(rnd, max_depth)
          else
            _JsonValueStringGen._gen_object(rnd, max_depth)
          end
      end)

class \nodoc\ iso _StreamMatchesBatchProperty is Property1[String]
  """Feeding a whole document to the stream parser yields the batch value."""
  fun name(): String => "json/stream/property/matches-batch"

  fun gen(): Generator[String] => _JsonDocStringGen(3)

  fun ref property(doc: String, ph: PropertyHelper) =>
    match JsonParser.parse(doc)
    | let bv: JsonValue =>
      let batch: String val = JsonPrinter.print(bv)
      let p = JsonStreamParser
      p.feed(doc)
      match p.pull()
      | let sv: JsonValue =>
        ph.assert_eq[String val](batch, JsonPrinter.print(sv))
        // a single document leaves the stream drained
        match p.pull()
        | JsonNeedMore => None
        else ph.fail("stream not drained after one document: " + doc)
        end
      | JsonNeedMore =>
        ph.fail("stream returned NeedMore on a complete document: " + doc)
      | let e: JsonParseError =>
        ph.fail("stream failed on " + doc + ": " + e.string())
      end
    | let e: JsonParseError =>
      ph.fail("batch failed on generated doc " + doc + ": " + e.string())
    end

class \nodoc\ iso _StreamSplitInvariantProperty is Property1[String]
  """The result is identical no matter where the document is split into chunks."""
  fun name(): String => "json/stream/property/split-invariant"

  fun gen(): Generator[String] => _JsonDocStringGen(3)

  fun ref property(doc: String, ph: PropertyHelper) =>
    match _StreamHelp.all(doc)
    | let whole_vs: Array[JsonValue] =>
      let whole: String val = _StreamHelp.render(whole_vs)
      for n in [as USize: 1; 2; 3].values() do
        match _StreamHelp.split(doc, n)
        | let vs: Array[JsonValue] =>
          ph.assert_eq[String val](whole, _StreamHelp.render(vs))
        | let e: JsonParseError =>
          ph.fail("split at " + n.string() + " failed on " + doc + ": "
            + e.string())
        end
      end
    | let e: JsonParseError =>
      ph.fail("whole-feed failed on generated doc " + doc + ": " + e.string())
    end
