use "pony_test"
use "pony_check"

primitive \nodoc\ _StreamHelp
  """Drives JsonTokenParser + JsonReassembler for the streaming tests."""

  fun collect(input: String, chunk: USize): (Array[JsonValue] | JsonParseError) =>
    """
    Feed `input` in chunks of `chunk` bytes through a token parser and reassembler,
    call `finish()`, and return every completed top-level value (or the error).
    """
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    try
      var i: USize = 0
      while i < input.size() do
        let upper = (i + chunk).min(input.size())
        p.feed(input.trim(i, upper))?
        i = upper
      end
      p.finish()?
    else
      return p.parse_error()
    end
    let out = Array[JsonValue]
    for v in re.take_values().values() do out.push(v) end
    out

  fun all(input: String): (Array[JsonValue] | JsonParseError) =>
    """Feed `input` as a single chunk."""
    collect(input, input.size().max(1))

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
      nav("c").as_null()?
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
  """A stream of top-level values yields each as it completes."""
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

class \nodoc\ iso _TestStreamScalarRoot is UnitTest
  """Top-level scalars are accepted (unlike the closed #5558 design)."""
  fun name(): String => "json/stream/scalar-root"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[I64](42, JsonNav(_one(h, "42")?).as_i64()?)
    h.assert_eq[F64](3.14, JsonNav(_one(h, "  3.14  ")?).as_f64()?)
    h.assert_eq[String]("hi", JsonNav(_one(h, "\"hi\"")?).as_string()?)
    h.assert_eq[Bool](true, JsonNav(_one(h, "true")?).as_bool()?)
    JsonNav(_one(h, "null")?).as_null()?
    // a stream of bare scalars
    match _StreamHelp.all("1 2 3")
    | let vs: Array[JsonValue] => h.assert_eq[USize](3, vs.size())
    | let e: JsonParseError => h.fail("scalar stream failed: " + e.string())
    end

  fun _one(h: TestHelper, doc: String): JsonValue ? =>
    match _StreamHelp.all(doc)
    | let vs: Array[JsonValue] =>
      h.assert_eq[USize](1, vs.size()); vs(0)?
    | let e: JsonParseError => h.fail(doc + ": " + e.string()); error
    end

class \nodoc\ iso _TestStreamFinishNumber is UnitTest
  """A bare trailing number is not emitted until finish() (no terminator)."""
  fun name(): String => "json/stream/finish-number"

  fun apply(h: TestHelper) ? =>
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    p.feed("42")?
    // The number has no following byte yet, so nothing has completed.
    h.assert_eq[USize](0, re.take_values().size())
    h.assert_true(p.incomplete())
    p.finish()?
    h.assert_false(p.incomplete())
    let vs = re.take_values()
    h.assert_eq[USize](1, vs.size())
    h.assert_eq[I64](42, JsonNav(vs(0)?).as_i64()?)

class \nodoc\ iso _TestStreamSplitInvariance is UnitTest
  """The result is identical no matter where chunk boundaries fall."""
  fun name(): String => "json/stream/split-invariance"

  fun apply(h: TestHelper) =>
    let input =
      """{"name":"alice","tags":["x","y"],"n":-12.5e3,"nested":{"deep":[true,null]},"u":"sm😀ile\n\t"} [1,2,{"k":false}] 99999999999999999999"""
    let whole =
      match _StreamHelp.all(input)
      | let vs: Array[JsonValue] => _StreamHelp.render(vs)
      | let e: JsonParseError => h.fail("whole parse failed: " + e.string()); ""
      end
    for n in [as USize: 1; 2; 3; 5; 7; 13].values() do
      match _StreamHelp.collect(input, n)
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
    // Every C-style escape, a BMP \uXXXX escape, and a \uXXXX surrogate pair,
    // decoded the same as the batch parser even when split one byte per feed —
    // which suspends the scanner mid-escape, mid-\uXXXX hex, and between the two
    // \u of the pair. (The triple-quoted literal is not escape-processed by Pony,
    // so \", \uD83D, etc. reach the parser verbatim.)
    let input = """{"s":"q\"b\\s\/f\b\f\n\r\t\u00E9-\uD83D\uDE00"}"""
    let expected =
      match JsonParser.parse(input)
      | let v: JsonValue => JsonNav(v)("s").as_string()?
      | let e: JsonParseError => h.fail("batch failed: " + e.string()); ""
      end
    match _StreamHelp.collect(input, 1)
    | let vs: Array[JsonValue] =>
      h.assert_eq[String](expected, JsonNav(vs(0)?)("s").as_string()?)
    | let e: JsonParseError => h.fail("stream failed: " + e.string())
    end

    // Invalid/lone surrogates are rejected too, including when the reject must
    // fire mid-resume (fed one byte per chunk).
    for bad in
      [ """["\uD800"]"""; """["\uDC00"]"""; """["\uD800A"]"""
        """["\uD83D\uD83D"]""" ].values()
    do
      match _StreamHelp.collect(bad, 1)
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
      h.assert_eq[F64](-2.5e-3, JsonNav(a(4)?).as_f64()?)
      h.assert_eq[F64](1e10, JsonNav(a(5)?).as_f64()?)
      h.assert_true(JsonNav(a(6)?).as_f64()? > 1.0e20)
    | let e: JsonParseError => h.fail("unexpected error: " + e.string())
    end

    // Malformed numbers (the over-consume-then-validate path) are rejected.
    for bad in
      [ "[1-2]"; "[1.2.3]"; "[1e]"; "[.5]"; "[+5]"; "[--1]"; "[1.]"; "[01]"
        "[1e+e1]" ].values()
    do
      match _StreamHelp.all(bad)
      | let vs: Array[JsonValue] => h.fail("expected a number error: " + bad)
      | let e: JsonParseError => None
      end
    end

class \nodoc\ iso _TestStreamTrailingComma is UnitTest
  fun name(): String => "json/stream/trailing-comma"

  fun apply(h: TestHelper) =>
    _expect_error(h, "[1,]")
    _expect_error(h, """{"a":1,}""")
    _expect_error(h, "[1,2,]")

  fun _expect_error(h: TestHelper, input: String) =>
    match _StreamHelp.all(input)
    | let vs: Array[JsonValue] => h.fail("expected error for: " + input)
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

class \nodoc\ iso _TestStreamErrorLatches is UnitTest
  """Once feed() raises, the parser stays failed on later feeds."""
  fun name(): String => "json/stream/error-latches"

  fun apply(h: TestHelper) =>
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    var first_raised = false
    try p.feed("""{"a":}""")? else first_raised = true end
    h.assert_true(first_raised, "malformed input should raise")
    var second_raised = false
    try p.feed("""{"b":2}""")? else second_raised = true end
    h.assert_true(second_raised, "error did not latch")

class \nodoc\ iso _TestStreamIncomplete is UnitTest
  """Truncated input leaves the parser incomplete; JsonParser reports it."""
  fun name(): String => "json/stream/incomplete"

  fun apply(h: TestHelper) =>
    // Each of these ends mid-value: an open frame, a mid-string, a mid-keyword,
    // and (`[12`) a number that finish() completes inside an array that never
    // closes. After feeding + finish(), the parser is incomplete and no top-level
    // value completed.
    for truncated in
      [ """{"a":"""; """["ab"""; "[tru"; "{"; "[1,"; "[12" ].values()
    do
      let re = JsonReassembler
      let p = JsonTokenParser(re)
      try p.feed(truncated)?; p.finish()?
      else h.fail("truncated input should not raise: " + truncated)
      end
      h.assert_true(p.incomplete(),
        "expected incomplete for: " + truncated)
      h.assert_eq[USize](0, re.take_values().size(),
        "no value should complete for: " + truncated)
      // JsonParser turns the same truncation into an error.
      match JsonParser.parse(truncated)
      | let v: JsonValue => h.fail("JsonParser accepted truncated: " + truncated)
      | let e: JsonParseError => None
      end
    end

class \nodoc\ iso _TestStreamErrorLocation is UnitTest
  """A parse error reports byte offset and line absolute across all fed chunks."""
  fun name(): String => "json/stream/error-location"

  fun apply(h: TestHelper) =>
    // The malformed byte ('9' where ':' is expected) is in the SECOND chunk, on
    // the second line. The reported offset must be at least the first chunk's
    // length — proving offsets accumulate across chunks, not reset.
    let first = "{\n  \"a\" "
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    var err: (JsonParseError | None) = None
    try
      p.feed(first)?
      p.feed("9}")?
    else
      err = p.parse_error()
    end
    match err
    | let e: JsonParseError =>
      h.assert_true(e.offset >= first.size(),
        "error offset " + e.offset.string() + " is not absolute across chunks")
      h.assert_eq[USize](2, e.line)
    | None => h.fail("expected a parse error")
    end

class \nodoc\ iso _TestStreamLimitDepth is UnitTest
  fun name(): String => "json/stream/limit-depth"

  fun apply(h: TestHelper) =>
    let re = JsonReassembler
    let p = JsonTokenParser(re, JsonParseLimits(where max_depth' = 3))
    var raised = false
    try p.feed("[[[[1]]]]")? else raised = true end // depth 4 > 3
    h.assert_true(raised, "expected a depth-limit error")
    // depth exactly at the limit is fine
    match _StreamHelp.all("[[[1]]]")
    | let vs: Array[JsonValue] => h.assert_eq[USize](1, vs.size())
    | let e: JsonParseError => h.fail("depth 3 should be allowed: " + e.string())
    end

class \nodoc\ iso _TestStreamLimits is UnitTest
  fun name(): String => "json/stream/limits"

  fun apply(h: TestHelper) =>
    // max_string_len: a string of exactly the limit is accepted, +1 is not.
    _accept(h, JsonParseLimits(where max_string_len' = 4), """["abcd"]""")
    _reject(h, JsonParseLimits(where max_string_len' = 4), """["abcde"]""")
    // max_number_len: exactly the limit accepted, +1 not.
    _accept(h, JsonParseLimits(where max_number_len' = 5), "[12345]")
    _reject(h, JsonParseLimits(where max_number_len' = 5), "[123456]")
    // A limit holds even when the value is split across chunks.
    let re = JsonReassembler
    let cp = JsonTokenParser(re, JsonParseLimits(where max_string_len' = 6))
    var raised = false
    try
      cp.feed("[\"abc")?  // 3 string bytes so far
      cp.feed("defg\"]")? // "abcdefg" is 7 > 6
    else raised = true
    end
    h.assert_true(raised, "max_string_len not enforced across a chunk boundary")

  fun _reject(h: TestHelper, limits: JsonParseLimits, input: String) =>
    match _reject_or_accept(limits, input)
    | let vs: Array[JsonValue] => h.fail("expected a limit error for: " + input)
    | let e: JsonParseError => None
    end

  fun _accept(h: TestHelper, limits: JsonParseLimits, input: String) =>
    match _reject_or_accept(limits, input)
    | let vs: Array[JsonValue] =>
      h.assert_true(vs.size() >= 1, "expected a value for: " + input)
    | let e: JsonParseError =>
      h.fail("unexpected error for " + input + ": " + e.string())
    end

  fun _reject_or_accept(limits: JsonParseLimits, input: String)
    : (Array[JsonValue] | JsonParseError)
  =>
    let re = JsonReassembler
    let p = JsonTokenParser(re, limits)
    try p.feed(input)?; p.finish()?
    else return p.parse_error()
    end
    let out = Array[JsonValue]
    for v in re.take_values().values() do out.push(v) end
    out

class \nodoc\ iso _TestStreamAbort is UnitTest
  """abort() from the notifier raises the current feed and latches."""
  fun name(): String => "json/stream/abort"

  fun apply(h: TestHelper) =>
    let p = JsonTokenParser(
      object is JsonTokenNotify
        var _count: USize = 0
        fun ref apply(parser': JsonTokenParser, token: JsonToken) =>
          _count = _count + 1
          if _count >= 2 then parser'.abort() end
      end)
    var raised = false
    try p.feed("[1,2,3]")? else raised = true end
    h.assert_true(raised, "abort should raise the current feed")
    // and it latches: a later feed raises too
    var second_raised = false
    try p.feed("[4,5]")? else second_raised = true end
    h.assert_true(second_raised, "abort did not latch")

class \nodoc\ iso _TestStreamTokens is UnitTest
  """The token sequence and each token's carried value."""
  fun name(): String => "json/stream/tokens"

  fun apply(h: TestHelper) =>
    let events = Array[String]
    let p = JsonTokenParser(_StreamTokenRecorder(events))
    try p.feed("""{"a":1,"b":[true,"x"],"c":-2.5}""")?; p.finish()?
    else h.fail("token parse raised"); return
    end
    let expected =
      [ "{"; "key:a"; "int:1"; "key:b"; "["; "true"; "str:x"; "]"
        "key:c"; "flt:-2.5"; "}" ]
    h.assert_eq[USize](expected.size(), events.size())
    for (idx, exp) in expected.pairs() do
      try h.assert_eq[String](exp, events(idx)?)
      else h.fail("missing event at " + idx.string())
      end
    end

class \nodoc\ iso _TestStreamFlatMemory is UnitTest
  """
  The design's headline: pull one field out of a large document with a raw
  notifier that materializes nothing — no reassembler, no tree.
  """
  fun name(): String => "json/stream/flat-memory"

  fun apply(h: TestHelper) =>
    // Build a large array of records; extract every "id" value without ever
    // holding a JsonValue.
    let doc = recover val
      let s = String
      s.append("""{"records":[""")
      var i: USize = 0
      while i < 500 do
        if i > 0 then s.push(',') end
        // A "score" number sits beside "id"; the collector must pick only ids,
        // so a broken key guard (collecting every number) would over-count.
        s.append("""{"id":""")
        s.append(i.string())
        s.append(""","score":""")
        s.append((i * 7).string())
        s.append(""","name":"item-""")
        s.append(i.string())
        s.append("\"}")
        i = i + 1
      end
      s.append("]}")
      s
    end

    // Feed in small chunks so the reader drains as it goes — the parser never
    // holds the whole document, which is the flat-memory property.
    let collector: _IdCollector ref = _IdCollector
    let p = JsonTokenParser(collector)
    try
      var off: USize = 0
      while off < doc.size() do
        let upper = (off + 64).min(doc.size())
        p.feed(doc.trim(off, upper))?
        off = upper
      end
      p.finish()?
    else h.fail("flat-memory parse raised"); return
    end
    h.assert_eq[USize](500, collector.ids.size())
    try
      h.assert_eq[I64](0, collector.ids(0)?)
      h.assert_eq[I64](499, collector.ids(499)?)
    end

class \nodoc\ iso _TestStreamDifferential is UnitTest
  """Chunked token+reassembler agrees with the whole-document JsonParser."""
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
      // Feed one byte per chunk to force resume at every boundary.
      match _StreamHelp.collect(doc, 1)
      | let vs: Array[JsonValue] =>
        if vs.size() == 1 then
          try h.assert_eq[String](batch, JsonPrinter.print(vs(0)?)) end
        else
          h.fail("expected exactly one value for " + doc)
        end
      | let e: JsonParseError => h.fail("stream failed on " + doc)
      end
    end

class \nodoc\ iso _TestStreamReassemblerReuse is UnitTest
  """values() drains completed values; a partial run stays buffered."""
  fun name(): String => "json/stream/reassembler-reuse"

  fun apply(h: TestHelper) ? =>
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    p.feed("""{"a":1} {"b":""")? // one complete value, one partial
    let first = re.take_values()
    h.assert_eq[USize](1, first.size())
    h.assert_true(re.mid_value(), "the second value is mid-flight")
    // draining again yields nothing new while the value is still open
    h.assert_eq[USize](0, re.take_values().size())
    p.feed("2}")?
    let second = re.take_values()
    h.assert_eq[USize](1, second.size())
    h.assert_false(re.mid_value())
    h.assert_eq[I64](2, JsonNav(second(0)?)("b").as_i64()?)

class \nodoc\ iso _TestStreamProtocol is UnitTest
  """feed/finish lifecycle edges."""
  fun name(): String => "json/stream/protocol"

  fun apply(h: TestHelper) ? =>
    // Empty feeds are harmless no-ops; a real chunk still parses.
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    p.feed("")?
    p.feed("[1]")?
    p.feed("")?
    p.finish()?
    let vs = re.take_values()
    h.assert_eq[USize](1, vs.size())
    h.assert_eq[USize](1, JsonNav(vs(0)?).as_array()?.size())

    // A complete value followed by garbage: the value completes, then the
    // trailing garbage latches an error.
    let re2 = JsonReassembler
    let p2 = JsonTokenParser(re2)
    var raised = false
    try p2.feed("[1,2,3] x")? else raised = true end
    h.assert_true(raised, "trailing garbage should raise")
    h.assert_eq[USize](1, re2.take_values().size(),
      "the leading value still completed")

class \nodoc\ iso _TestStreamReassemblerAdd is UnitTest
  """A hand-built token run assembles the same value as a parsed one."""
  fun name(): String => "json/stream/reassembler-add"

  fun apply(h: TestHelper) ? =>
    // {"a":[1,true],"b":{}} built by handing tokens straight to add().
    let re = JsonReassembler
    re.add(JsonTokenObjectStart)
    re.add(JsonTokenKey("a"))
    re.add(JsonTokenArrayStart)
    re.add(JsonTokenNumber(I64(1)))
    re.add(JsonTokenTrue)
    re.add(JsonTokenArrayEnd)
    re.add(JsonTokenKey("b"))
    re.add(JsonTokenObjectStart)
    re.add(JsonTokenObjectEnd)
    re.add(JsonTokenObjectEnd)
    let vs = re.take_values()
    h.assert_eq[USize](1, vs.size())
    let nav = JsonNav(vs(0)?)
    h.assert_eq[I64](1, nav("a")(USize(0)).as_i64()?)
    h.assert_eq[Bool](true, nav("a")(USize(1)).as_bool()?)
    h.assert_eq[USize](0, nav("b").as_object()?.size())

    // A partial run leaves mid_value() true and yields nothing until it closes.
    let re2 = JsonReassembler
    re2.add(JsonTokenArrayStart)
    re2.add(JsonTokenNumber(I64(9)))
    h.assert_true(re2.mid_value())
    h.assert_eq[USize](0, re2.take_values().size())
    re2.add(JsonTokenArrayEnd)
    h.assert_false(re2.mid_value())
    h.assert_eq[USize](1, re2.take_values().size())

class \nodoc\ iso _TestStreamFinishInvalidNumber is UnitTest
  """finish() rejects a bare trailing number whose text is not valid."""
  fun name(): String => "json/stream/finish-invalid-number"

  fun apply(h: TestHelper) =>
    for bad in [ "1."; "1e"; "-"; "1e+" ].values() do
      let re = JsonReassembler
      let p = JsonTokenParser(re)
      var raised = false
      try p.feed(bad)?; p.finish()? else raised = true end
      h.assert_true(raised,
        "finish() should reject bare malformed number: " + bad)
    end

class \nodoc\ iso _TestStreamFinishLatches is UnitTest
  """After finish(), the parser is done; feeding or finishing again raises."""
  fun name(): String => "json/stream/finish-latches"

  fun apply(h: TestHelper) =>
    let re = JsonReassembler
    let p = JsonTokenParser(re)
    try p.feed("1")?; p.finish()? else h.fail("clean parse should not raise") end
    var feed_raised = false
    try p.feed("234")? else feed_raised = true end
    h.assert_true(feed_raised, "feed after finish() should raise")
    var finish_raised = false
    try p.finish()? else finish_raised = true end
    h.assert_true(finish_raised, "finish after finish() should raise")

class \nodoc\ iso _TestStreamNumberLimitSplit is UnitTest
  """max_number_len holds when the number is split across a chunk boundary."""
  fun name(): String => "json/stream/number-limit-split"

  fun apply(h: TestHelper) =>
    let re = JsonReassembler
    let p = JsonTokenParser(re, JsonParseLimits(where max_number_len' = 5))
    var raised = false
    try
      p.feed("[123")?  // 3 number bytes so far
      p.feed("456]")?  // 123456 is 6 > 5
    else raised = true
    end
    h.assert_true(raised, "max_number_len not enforced across a chunk boundary")

class \nodoc\ iso _TestStreamEndAnchorSplit is UnitTest
  """
  An empty container's end token anchors at the closing bracket even when the
  bracket arrives in a later feed than the opening one.
  """
  fun name(): String => "json/stream/end-anchor-split"

  fun apply(h: TestHelper) =>
    let rec: _EndAnchorRecorder ref = _EndAnchorRecorder
    let p = JsonTokenParser(rec)
    // "{" then "  }" — the '}' is at absolute offset 3.
    try p.feed("{")?; p.feed("  }")?; p.finish()?
    else h.fail("parse raised"); return
    end
    h.assert_eq[USize](3, rec.end_start, "ObjectEnd should anchor at the '}'")
    h.assert_eq[USize](4, rec.end_end)

class \nodoc\ iso _TestStreamReentrancyGuarded is UnitTest
  """A notifier that re-enters feed() is rejected, and the parse is unharmed."""
  fun name(): String => "json/stream/reentrancy-guarded"

  fun apply(h: TestHelper) =>
    let events = Array[String]
    let rec = _ReentrantRecorder(events)
    let p = JsonTokenParser(rec)
    try p.feed("[1,2,3]")?; p.finish()? else h.fail("parse raised") end
    // the re-entrant feed was rejected, so the token stream is intact: [ 1 2 3 ]
    h.assert_eq[USize](5, events.size())
    h.assert_true(rec.reentry_rejected, "re-entrant feed should have raised")

class \nodoc\ iso _TestStreamLargeChunked is UnitTest
  """
  A large string value fed in tiny chunks reassembles correctly — the reader's
  scan cursor walks, and its chunks drop, across many nodes.
  """
  fun name(): String => "json/stream/large-chunked"

  fun apply(h: TestHelper) =>
    let content = recover val
      let b = String
      var i: USize = 0
      while i < 4000 do b.push('x'); i = i + 1 end
      b
    end
    let doc: String = "[\"" + content + "\"]"
    match _StreamHelp.collect(doc, 2) // 2-byte chunks
    | let vs: Array[JsonValue] =>
      try
        h.assert_eq[USize](1, vs.size())
        h.assert_eq[USize](4000, JsonNav(vs(0)?)(USize(0)).as_string()?.size())
      end
    | let e: JsonParseError => h.fail("large chunked parse failed: " + e.string())
    end

class \nodoc\ iso _TestStreamZeroCopyView is UnitTest
  """
  A plain string that fits in one fed chunk is a zero-copy view into that chunk;
  an escaped string is a decoded copy. Verified by checking whether the value's
  bytes point inside the source buffer.
  """
  fun name(): String => "json/stream/zero-copy-view"

  fun apply(h: TestHelper) =>
    let src: String = """["plain","esc\ndecoded"]"""
    let probe = _ViewProbe(src.array().cpointer().usize(), src.size())
    let p = JsonTokenParser(probe)
    try p.feed(src)?; p.finish()? else h.fail("parse raised"); return end
    h.assert_eq[USize](2, probe.results.size())
    try
      // "plain" — no escape, one chunk → a view, so its bytes are inside src
      h.assert_true(probe.results(0)?, "plain string should be a zero-copy view")
      // "esc\ndecoded" — has an escape → a copy, so its bytes are elsewhere
      h.assert_false(probe.results(1)?, "escaped string should be a copy")
    end

class \nodoc\ _ViewProbe is JsonTokenNotify
  """Records, per string token, whether its bytes lie inside the source buffer."""
  let results: Array[Bool] = Array[Bool]
  let _base: USize
  let _len: USize

  new create(base: USize, len: USize) =>
    _base = base
    _len = len

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    match token
    | let s: JsonTokenString =>
      let addr = s.value.cpointer().usize()
      results.push((addr >= _base) and (addr < (_base + _len)))
    end

class \nodoc\ _EndAnchorRecorder is JsonTokenNotify
  """Captures the byte span of the last object-end token."""
  var end_start: USize = 0
  var end_end: USize = 0

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    match token
    | JsonTokenObjectEnd =>
      end_start = parser.token_start()
      end_end = parser.token_end()
    end

class \nodoc\ _ReentrantRecorder is JsonTokenNotify
  """Records a token per call and tries (and fails) to re-enter feed()."""
  let _events: Array[String]
  var reentry_rejected: Bool = false

  new create(events: Array[String]) =>
    _events = events

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    _events.push("t")
    // Re-entering feed() while the parse is running must raise, not corrupt it.
    try parser.feed("99")? else reentry_rejected = true end

class \nodoc\ _StreamTokenRecorder is JsonTokenNotify
  """Records a label (with carried value) per token."""
  let _events: Array[String]

  new create(events: Array[String]) =>
    _events = events

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    _events.push(
      match token
      | JsonTokenObjectStart => "{"
      | JsonTokenObjectEnd => "}"
      | JsonTokenArrayStart => "["
      | JsonTokenArrayEnd => "]"
      | JsonTokenTrue => "true"
      | JsonTokenFalse => "false"
      | JsonTokenNull => "null"
      | let k: JsonTokenKey => "key:" + k.value
      | let s: JsonTokenString => "str:" + s.value
      | let n: JsonTokenNumber =>
        match n.value
        | let i: I64 => "int:" + i.string()
        | let f: F64 => "flt:" + f.string()
        end
      end)

class \nodoc\ _IdCollector is JsonTokenNotify
  """Collects every value that follows an "id" key, holding no JsonValue."""
  let ids: Array[I64] = Array[I64]
  var _want: Bool = false

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    match token
    | let k: JsonTokenKey =>
      _want = k.value == "id"
    | let n: JsonTokenNumber =>
      if _want then
        match n.value | let i: I64 => ids.push(i) | let f: F64 => None end
      end
      _want = false
    else
      _want = false
    end

class \nodoc\ iso _StreamMatchesBatchProperty is Property1[String]
  """Chunked token+reassembler yields the same value as the batch parser."""
  fun name(): String => "json/stream/property/matches-batch"

  fun gen(): Generator[String] => _JsonDocStringGen(3)

  fun ref property(doc: String, ph: PropertyHelper) =>
    match JsonParser.parse(doc)
    | let bv: JsonValue =>
      let batch: String val = JsonPrinter.print(bv)
      // chunk size 2 exercises resume without being as slow as size 1
      match _StreamHelp.collect(doc, 2)
      | let vs: Array[JsonValue] =>
        if vs.size() == 1 then
          try ph.assert_eq[String val](batch, JsonPrinter.print(vs(0)?)) end
        else
          ph.fail("expected one value for: " + doc)
        end
      | let e: JsonParseError =>
        ph.fail("stream failed on " + doc + ": " + e.string())
      end
    | let e: JsonParseError =>
      ph.fail("batch failed on generated doc " + doc + ": " + e.string())
    end

class \nodoc\ iso _StreamSplitInvariantProperty is Property1[String]
  """The result is identical no matter where the document is split."""
  fun name(): String => "json/stream/property/split-invariant"

  fun gen(): Generator[String] => _JsonDocStringGen(3)

  fun ref property(doc: String, ph: PropertyHelper) =>
    match _StreamHelp.all(doc)
    | let whole_vs: Array[JsonValue] =>
      let whole: String val = _StreamHelp.render(whole_vs)
      for n in [as USize: 1; 2; 3].values() do
        match _StreamHelp.collect(doc, n)
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

primitive \nodoc\ _JsonDocStringGen
  """
  Generates a JSON document whose root is an object or array, reusing the
  package's value-string generator.
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
