use "pony_test"
use "pony_check"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // Property tests
    test(Property1UnitTest[I64](_ArrayPushApplyProperty))
    test(Property1UnitTest[I64](_ArrayPushPopProperty))
    test(Property1UnitTest[USize](_ArraySizeProperty))
    test(Property1UnitTest[F64](_F64RoundtripProperty))
    test(Property1UnitTest[String](_FilterSafetyProperty))
    test(Property1UnitTest[(String, String)](
      _FunctionCountLengthEquivalenceProperty))
    test(Property1UnitTest[(String, String)](
      _FunctionMatchImpliesSearchProperty))
    test(Property1UnitTest[(String, String)](_FunctionSafetyProperty))
    test(Property1UnitTest[I64](_I64RoundtripProperty))
    test(Property1UnitTest[String](_JSONPathSafetyProperty))
    test(Property1UnitTest[String](_ObjectRemoveProperty))
    test(Property1UnitTest[(String, String)](_ObjectSizeProperty))
    test(Property1UnitTest[(String, I64)](_ObjectUpdateApplyProperty))
    test(Property1UnitTest[String](_ParsePrintRoundtripProperty))
    test(Property1UnitTest[String](_StringEscapeRoundtripProperty))
    // Example tests
    test(_TestArrayUpdate)
    test(_TestJSONPathFilterAbsoluteQuery)
    test(_TestJSONPathFilterComparison)
    test(_TestJSONPathFilterDeepEquality)
    test(_TestJSONPathFilterExistence)
    test(_TestJSONPathFilterLogical)
    test(_TestJSONPathFilterFunctionCount)
    test(_TestJSONPathFilterFunctionLength)
    test(_TestJSONPathFilterFunctionMatchSearch)
    test(_TestJSONPathFilterFunctionParse)
    test(_TestJSONPathFilterFunctionValue)
    test(_TestJSONPathFilterNested)
    test(_TestJSONPathFilterNothing)
    test(_TestJSONPathFilterNumbers)
    test(_TestJSONPathFilterOnObjects)
    test(_TestJSONPathFilterParse)
    test(_TestJSONPathFilterTypes)
    test(_TestJSONPathParse)
    test(_TestJSONPathParseErrors)
    test(_TestJSONPathQueryAdvanced)
    test(_TestJSONPathQueryBasic)
    test(_TestJSONPathQueryComplex)
    test(_TestJSONPathQuerySliceStep)
    test(_TestLensComposition)
    test(_TestLensGet)
    test(_TestLensRemove)
    test(_TestLensSet)
    test(_TestNavInspection)
    test(_TestNavNotFound)
    test(_TestNavSuccess)
    test(_TestObjectGetOrElse)
    test(_TestParseContainers)
    test(_TestParseErrorLoneSurrogates)
    test(_TestParseErrors)
    test(_TestParseKeywords)
    test(_TestParseNumberOutOfRange)
    test(_TestParseNumbers)
    test(_TestParseStrings)
    test(_TestParseWholeDocument)
    test(_TestPrintCompact)
    test(_TestPrintFloats)
    test(_TestPrintFloatTypePreservation)
    test(_TestPrintNonFinite)
    test(_TestPrintPretty)
    test(_TestPrinterPretty)
    test(_TestPrinterScalars)
    test(_TestTokenParserAbort)
    // Regression tests — stack-safe JSON walks (issue #5557)
    test(_TestParseDeeplyNested)
    test(_TestPrintDeeplyNested)
    test(_TestJSONPathDescendDeeplyNested)
    test(_TestJSONPathDescendOrder)
    test(_TestJSONPathFilterDeeplyNested)
    test(_TestTokenParserPositions)
    test(_TestTokenParserEndPosition)
    test(_TestTokenParserStringPosition)
    // Streaming token parser + reassembler
    test(_TestStreamObject)
    test(_TestStreamArray)
    test(_TestStreamEmptyContainers)
    test(_TestStreamMultiValue)
    test(_TestStreamScalarRoot)
    test(_TestStreamFinishNumber)
    test(_TestStreamSplitInvariance)
    test(_TestStreamEscapes)
    test(_TestStreamNumbers)
    test(_TestStreamTrailingComma)
    test(_TestStreamMalformed)
    test(_TestStreamErrorLatches)
    test(_TestStreamIncomplete)
    test(_TestStreamErrorLocation)
    test(_TestStreamLimitDepth)
    test(_TestStreamLimits)
    test(_TestStreamAbort)
    test(_TestStreamTokens)
    test(_TestStreamFlatMemory)
    test(_TestStreamDifferential)
    test(_TestStreamReassemblerReuse)
    test(_TestStreamProtocol)
    test(_TestStreamReassemblerAdd)
    test(_TestStreamFinishInvalidNumber)
    test(_TestStreamFinishLatches)
    test(_TestStreamNumberLimitSplit)
    test(_TestStreamEndAnchorSplit)
    test(_TestStreamReentrancyGuarded)
    test(_TestStreamZeroCopyView)
    test(_TestStreamLargeChunked)
    test(Property1UnitTest[String](_StreamMatchesBatchProperty))
    test(Property1UnitTest[String](_StreamSplitInvariantProperty))

// ===================================================================
// Generators
// ===================================================================
primitive \nodoc\ _JSONValueStringGen
  """
  Generates valid JSON text strings with depth-bounded recursion.
  Produces strings like "42", "\"hello\"", "[1,true]", "{\"a\":1}".
  """
  fun apply(max_depth: USize = 2): Generator[String] =>
    let that = this
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String =>
          that._gen_value(rnd, max_depth)
      end)

  fun _gen_value(rnd: Randomness, depth: USize): String =>
    let choice =
      if depth == 0 then
        rnd.usize(0, 4)
      else
        rnd.usize(0, 6)
      end
    match choice
    | 0 => _gen_int(rnd)
    | 1 => _gen_float(rnd)
    | 2 => if rnd.bool() then "true" else "false" end
    | 3 => "null"
    | 4 => _gen_string(rnd)
    | 5 => _gen_object(rnd, depth - 1)
    | 6 => _gen_array(rnd, depth - 1)
    else "null"
    end

  fun _gen_int(rnd: Randomness): String =>
    rnd.i64(-1000, 1000).string()

  fun _gen_float(rnd: Randomness): String =>
    let numerator = rnd.i64(-100, 100)
    let denom: I64 =
      match rnd.usize(0, 3)
      | 0 => 2
      | 1 => 4
      | 2 => 5
      else 10
      end
    let f = numerator.f64() / denom.f64()
    let s: String val = f.string()
    if
      (not s.contains(".")) and
        (not s.contains("e")) and
        (not s.contains("E"))
    then
      s + ".0"
    else
      s
    end

  fun _gen_string(rnd: Randomness): String =>
    let len = rnd.usize(0, 15)
    var buf: String ref = String(len + 2)
    buf.push('"')
    var i: USize = 0
    while i < len do
      // Mix in escape and \uXXXX/surrogate sequences so property tests exercise
      // the escape and unicode decode paths (and their resume across chunks),
      // not just plain ASCII. Every branch is valid JSON.
      match rnd.usize(0, 9)
      | 0 => buf.append("\\n")
      | 1 => buf.append("\\t")
      | 2 => buf.append("\\r")
      | 3 => buf.append("\\u00e9")        // a BMP \uXXXX escape
      | 4 => buf.append("\\uD83D\\uDE00") // a surrogate pair
      else
        let c = rnd.u8(0x20, 0x7E)
        if c == '"' then
          buf.append("\\\"")
        elseif c == '\\' then
          buf.append("\\\\")
        else
          buf.push(c)
        end
      end
      i = i + 1
    end
    buf.push('"')
    buf.clone()

  fun _gen_object(rnd: Randomness, depth: USize): String =>
    let count = rnd.usize(0, 3)
    if count == 0 then return "{}" end
    var buf: String ref = String(64)
    buf.push('{')
    var i: USize = 0
    while i < count do
      if i > 0 then buf.push(',') end
      // generate a simple key
      let key_len = rnd.usize(1, 6)
      buf.push('"')
      var k: USize = 0
      while k < key_len do
        buf.push(rnd.u8('a', 'z'))
        k = k + 1
      end
      buf.push('"')
      buf.push(':')
      buf.append(_gen_value(rnd, depth))
      i = i + 1
    end
    buf.push('}')
    buf.clone()

  fun _gen_array(rnd: Randomness, depth: USize): String =>
    let count = rnd.usize(0, 4)
    if count == 0 then return "[]" end
    var buf: String ref = String(64)
    buf.push('[')
    var i: USize = 0
    while i < count do
      if i > 0 then buf.push(',') end
      buf.append(_gen_value(rnd, depth))
      i = i + 1
    end
    buf.push(']')
    buf.clone()

// ===================================================================
// Property Tests — Roundtrip
// ===================================================================
class \nodoc\ iso _ParsePrintRoundtripProperty is Property1[String]
  fun name(): String => "json/roundtrip/compact"

  fun gen(): Generator[String] =>
    _JSONValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // compact(parse(s)) is a fixpoint after one cycle
    let first_parse = JSONParser.parse(sample)
    match \exhaustive\ first_parse
    | let j1: JSONValue =>
      let s1: String val = JSONPrinter.print(j1)
      match \exhaustive\ JSONParser.parse(s1)
      | let j2: JSONValue =>
        let s2: String val = JSONPrinter.print(j2)
        ph.assert_eq[String val](s1, s2)
      | let e: JSONParseError =>
        ph.fail("Re-parse failed: " + e.string())
      end
    | let e: JSONParseError =>
      ph.fail("Initial parse failed for: " + sample + " — " + e.string())
    end

class \nodoc\ iso _I64RoundtripProperty is Property1[I64]
  fun name(): String => "json/roundtrip/i64"

  fun gen(): Generator[I64] =>
    // Restrict to 18-digit range: values with 19+ digits are promoted to F64
    // by the parser to avoid silent I64 overflow
    Generators.i64(-999_999_999_999_999_999, 999_999_999_999_999_999)

  fun ref property(sample: I64, ph: PropertyHelper) =>
    let s: String val = sample.string()
    match \exhaustive\ JSONParser.parse(s)
    | let j: JSONValue =>
      try
        let parsed = j as I64
        ph.assert_eq[I64](sample, parsed)
      else
        ph.fail("Parsed as wrong type for: " + s)
      end
    | let e: JSONParseError =>
      ph.fail("Parse failed for: " + s + " — " + e.string())
    end

class \nodoc\ iso _F64RoundtripProperty is Property1[F64]
  fun name(): String => "json/roundtrip/f64"

  fun params(): PropertyParams =>
    // Random doubles overwhelmingly need 16-17 significant digits, so a larger
    // sample densely exercises the full-precision range.
    PropertyParams(where num_samples' = 1000)

  fun gen(): Generator[F64] =>
    // Build a finite F64 from raw IEEE-754 fields. A uniform exponent in
    // [0, 2046] spans denormals (0) through the largest finite magnitude and
    // never selects 2047 (infinity/NaN); a full 52-bit mantissa reaches the
    // long-digit values that most stress the printer's precision.
    Generator[F64](
      object is GenObj[F64]
        fun generate(rnd: Randomness): F64 =>
          let sign = rnd.u64(0, 1) << 63
          let exp = rnd.u64(0, 2046) << 52
          let mant = rnd.u64() and 0x000F_FFFF_FFFF_FFFF
          F64.from_bits(sign or exp or mant)
      end)

  fun ref property(sample: F64, ph: PropertyHelper) =>
    // Serialize as a JSON array element, then recover it.
    let arr = JSONArray.push(sample)
    let s: String val = JSONPrinter.print(arr)
    match \exhaustive\ JSONParser.parse(s)
    | let j: JSONValue =>
      try
        // `as F64` also asserts the value re-parses as a float, not an integer.
        let parsed = (j as JSONArray)(0)? as F64
        // Compare bits, not values: F64 `==` treats -0.0 and 0.0 as equal and
        // would hide a lost sign. The round-trip must preserve every bit.
        ph.assert_eq[U64](sample.bits(), parsed.bits())
      else
        ph.fail("Roundtrip did not yield an F64; sample bits=" +
          sample.bits().string() + " printed=" + s)
      end
    | let e: JSONParseError =>
      ph.fail("Parse failed; sample bits=" + sample.bits().string() +
        " printed=" + s + " — " + e.string())
    end

class \nodoc\ iso _StringEscapeRoundtripProperty is Property1[String]
  fun name(): String => "json/roundtrip/string-escape"

  fun gen(): Generator[String] =>
    Generators.ascii(0, 50)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // Embed string in a JSON array, serialize, parse, extract
    let arr = JSONArray.push(sample)
    let serialized: String val = JSONPrinter.print(arr)
    match \exhaustive\ JSONParser.parse(serialized)
    | let j: JSONValue =>
      try
        let parsed_arr = j as JSONArray
        let recovered = parsed_arr(0)? as String
        ph.assert_eq[String val](sample, recovered)
      else
        ph.fail("Type mismatch in string roundtrip")
      end
    | let e: JSONParseError =>
      ph.fail("Parse failed: " + e.string())
    end

// ===================================================================
// Property Tests — JSONObject
// ===================================================================
class \nodoc\ iso _ObjectUpdateApplyProperty is Property1[(String, I64)]
  fun name(): String => "json/object/update-apply"

  fun gen(): Generator[(String, I64)] =>
    Generators.zip2[String, I64](
      Generators.ascii_letters(1, 10),
      Generators.i64(-1000, 1000))

  fun ref property(sample: (String, I64), ph: PropertyHelper) ? =>
    (let key, let value) = sample
    let obj = JSONObject.update(key, value)
    let got = obj(key)? as I64
    ph.assert_eq[I64](value, got)

class \nodoc\ iso _ObjectRemoveProperty is Property1[String]
  fun name(): String => "json/object/remove"

  fun gen(): Generator[String] =>
    Generators.ascii_letters(1, 10)

  fun ref property(sample: String, ph: PropertyHelper) =>
    let obj = JSONObject.update(sample, I64(42))
    ph.assert_true(obj.contains(sample))
    let removed = obj.remove(sample)
    ph.assert_false(removed.contains(sample))

class \nodoc\ iso _ObjectSizeProperty is Property1[(String, String)]
  fun name(): String => "json/object/size"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      Generators.ascii_letters(1, 10),
      Generators.ascii_letters(1, 10))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let k1, let k2) = sample
    // Update with first key — size is 1
    let obj1 = JSONObject.update(k1, I64(1))
    ph.assert_eq[USize](1, obj1.size())

    // Update same key — size stays 1
    let obj2 = obj1.update(k1, I64(2))
    ph.assert_eq[USize](1, obj2.size())

    // Update with different key — size depends on whether keys are equal
    let obj3 = obj1.update(k2, I64(3))
    if k1 == k2 then
      ph.assert_eq[USize](1, obj3.size())
    else
      ph.assert_eq[USize](2, obj3.size())
    end

// ===================================================================
// Property Tests — JSONArray
// ===================================================================
class \nodoc\ iso _ArrayPushApplyProperty is Property1[I64]
  fun name(): String => "json/array/push-apply"

  fun gen(): Generator[I64] =>
    Generators.i64()

  fun ref property(sample: I64, ph: PropertyHelper) ? =>
    let arr = JSONArray.push(sample)
    let got = arr(arr.size() - 1)? as I64
    ph.assert_eq[I64](sample, got)

class \nodoc\ iso _ArrayPushPopProperty is Property1[I64]
  fun name(): String => "json/array/push-pop"

  fun gen(): Generator[I64] =>
    Generators.i64()

  fun ref property(sample: I64, ph: PropertyHelper) ? =>
    let base = JSONArray.push(I64(99))
    let extended = base.push(sample)
    (let popped, let value) = extended.pop()?
    let got = value as I64
    ph.assert_eq[I64](sample, got)
    ph.assert_eq[USize](base.size(), popped.size())

class \nodoc\ iso _ArraySizeProperty is Property1[USize]
  fun name(): String => "json/array/size"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 20)

  fun ref property(sample: USize, ph: PropertyHelper) =>
    var arr = JSONArray
    var i: USize = 0
    while i < sample do
      arr = arr.push(I64(i.i64()))
      i = i + 1
    end
    ph.assert_eq[USize](sample, arr.size())

// ===================================================================
// Property Tests — JSONPath Safety
// ===================================================================
class \nodoc\ iso _JSONPathSafetyProperty is Property1[String]
  fun name(): String => "json/jsonpath/safety"

  fun gen(): Generator[String] =>
    _JSONValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // Parse the generated JSON
    match \exhaustive\ JSONParser.parse(sample)
    | let doc: JSONValue =>
      // A set of valid paths — none should crash
      let paths: Array[String] val =
        [
          "$"
          "$.*"
          "$.a"
          "$[0]"
          "$[-1]"
          "$[*]"
          "$..a"
          "$..*"
          "$[0:2]"
          "$[:2]"
          "$[1:]"
          "$.a.b"
          "$.a[0]"
          "$[0,1]"
          "$[0:2:1]"
          "$[::2]"
          "$[::-1]"
          "$[::0]"
          "$[1::-1]"
          "$[?@.a]"
          "$[?@.a > 0]"
          "$[?@.a == 1 && @.b == 2]"
          "$[?length(@.a) > 0]"
          """$[?match(@.a, "[a-z]")]"""
          """$[?search(@.a, "test")]"""
        ]
      for path_str in paths.values() do
        try
          let path = JSONPathParser.compile(path_str)?
          // query should never crash — it returns an array (possibly empty)
          let results = path.query(doc)
          // Just verify we got an array back (size >= 0)
          ph.assert_true(results.size() >= 0)
        else
          ph.fail("Failed to compile known-valid path: " + path_str)
        end
      end
    | let _: JSONParseError =>
      // Generator produced invalid JSON — shouldn't happen but skip
      None
    end

// ===================================================================
// Example Tests — Parser Success
// ===================================================================
class \nodoc\ iso _TestParseKeywords is UnitTest
  fun name(): String => "json/parse/keywords"

  fun apply(h: TestHelper) ? =>
    match JSONParser.parse("true")
    | let j: JSONValue => h.assert_eq[Bool](true, j as Bool)
    else h.fail("true failed to parse")
    end

    match JSONParser.parse("false")
    | let j: JSONValue => h.assert_eq[Bool](false, j as Bool)
    else h.fail("false failed to parse")
    end

    match JSONParser.parse("null")
    | let j: JSONValue =>
      match j
      | None => None // pass
      else h.fail("null parsed as wrong type")
      end
    else h.fail("null failed to parse")
    end

class \nodoc\ iso _TestParseNumbers is UnitTest
  fun name(): String => "json/parse/numbers"

  fun apply(h: TestHelper) ? =>
    // Integers
    match JSONParser.parse("0")
    | let j: JSONValue => h.assert_eq[I64](0, j as I64)
    else h.fail("0 failed")
    end

    match JSONParser.parse("42")
    | let j: JSONValue => h.assert_eq[I64](42, j as I64)
    else h.fail("42 failed")
    end

    match JSONParser.parse("-1")
    | let j: JSONValue => h.assert_eq[I64](-1, j as I64)
    else h.fail("-1 failed")
    end

    // Floats
    match JSONParser.parse("3.14")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - 3.14).abs() < 1e-10)
    else h.fail("3.14 failed")
    end

    match JSONParser.parse("1e10")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - 1e10).abs() < 1.0)
    else h.fail("1e10 failed")
    end

    match JSONParser.parse("1.5e-3")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - 0.0015).abs() < 1e-10)
    else h.fail("1.5e-3 failed")
    end

    match JSONParser.parse("-0.5")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - (-0.5)).abs() < 1e-10)
    else h.fail("-0.5 failed")
    end

    // Large integer promoted to F64 instead of overflowing
    match JSONParser.parse("99999999999999999999")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f > 9.99e18)
    else h.fail("large integer failed")
    end

    // The I64/F64 promotion boundary: 18 digits stays an integer, 19 becomes a
    // float even though it would still fit I64.
    match JSONParser.parse("999999999999999999") // 18 nines
    | let j: JSONValue => h.assert_eq[I64](999999999999999999, j as I64)
    else h.fail("18-digit integer failed")
    end

    match JSONParser.parse("1000000000000000000") // 19 digits
    | let j: JSONValue => h.assert_true((j as F64).finite())
    else h.fail("19-digit integer failed")
    end

    // Zero alone is valid
    match JSONParser.parse("0")
    | let j: JSONValue => h.assert_eq[I64](0, j as I64)
    else h.fail("standalone 0 failed")
    end

    // 0.5 is valid (zero before decimal)
    match JSONParser.parse("0.5")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - 0.5).abs() < 1e-10)
    else h.fail("0.5 failed")
    end

    // A zero written with a large exponent is still zero.
    match JSONParser.parse("0e309")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f.finite())
      h.assert_eq[F64](0, f)
    else h.fail("0e309 failed")
    end

    match JSONParser.parse("0.0e999")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f.finite())
      h.assert_eq[F64](0, f)
    else h.fail("0.0e999 failed")
    end

    match JSONParser.parse("-0e309")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f.finite())
      h.assert_eq[F64](0, f)
    else h.fail("-0e309 failed")
    end

    // The largest power-of-ten literal that stays finite; 1e309 overflows.
    match JSONParser.parse("1e308")
    | let j: JSONValue => h.assert_true((j as F64).finite())
    else h.fail("1e308 failed")
    end

    // In range, but an intermediate would over- or underflow if the value were
    // built digit by digit, so the conversion must be correctly rounded: here
    // 1e320 (integer part, over F64) times 1e-310 (exponent) is 1e10.
    match JSONParser.parse("1" + "0".mul(320) + "e-310")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f.finite())
      h.assert_true((f - 1e10).abs() < 1.0)
    else h.fail("large integer with negative exponent failed")
    end

    // A nonzero fraction whose leading zeros underflow if summed term by term,
    // pulled back into range by the exponent.
    match JSONParser.parse("0." + "0".mul(320) + "1e320") // ~0.1
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true((f - 0.1).abs() < 1e-9)
    else h.fail("tiny fraction with large exponent failed")
    end

    // Below the smallest positive F64: underflows to 0 and is accepted, not
    // rejected as out of range.
    match JSONParser.parse("1e-400")
    | let j: JSONValue =>
      let f = j as F64
      h.assert_true(f.finite())
      h.assert_eq[F64](0, f)
    else h.fail("1e-400 failed")
    end

class \nodoc\ iso _TestParseNumberOutOfRange is UnitTest
  fun name(): String => "json/parse/number-out-of-range"

  fun apply(h: TestHelper) =>
    _assert_out_of_range(h, "1e999")
    _assert_out_of_range(h, "-1e999")
    _assert_out_of_range(h, "1e400")
    _assert_out_of_range(h, "2e308")
    _assert_out_of_range(h, "[1e999]")
    _assert_out_of_range(h, "1" + "0".mul(309))

  fun _assert_out_of_range(h: TestHelper, input: String) =>
    match \exhaustive\ JSONParser.parse(input)
    | let e: JSONParseError =>
      h.assert_eq[String]("Number out of range", e.message)
    | let _: JSONValue => h.fail("Expected out-of-range error for: " + input)
    end

class \nodoc\ iso _TestParseStrings is UnitTest
  fun name(): String => "json/parse/strings"

  fun apply(h: TestHelper) ? =>
    // Simple string
    match JSONParser.parse("\"hello\"")
    | let j: JSONValue => h.assert_eq[String]("hello", j as String)
    else h.fail("simple string failed")
    end

    // All basic escape sequences
    match JSONParser.parse("\"a\\nb\\tc\\\"d\\\\e\\/f\"")
    | let j: JSONValue =>
      let s = j as String
      h.assert_eq[String]("a\nb\tc\"d\\e/f", s)
    else h.fail("escape sequences failed")
    end

    // \b and \f
    match JSONParser.parse("\"\\b\\f\"")
    | let j: JSONValue =>
      let s = j as String
      h.assert_eq[U8](0x08, try s(0)? else 0 end)
      h.assert_eq[U8](0x0C, try s(1)? else 0 end)
    else h.fail("\\b\\f failed")
    end

    // \r
    match JSONParser.parse("\"\\r\"")
    | let j: JSONValue =>
      h.assert_eq[String]("\r", j as String)
    else h.fail("\\r failed")
    end

    // Unicode BMP: \u00E9 = é
    match JSONParser.parse("\"\\u00E9\"")
    | let j: JSONValue =>
      let s = j as String
      let expected = recover val String.from_utf32(0xE9) end
      h.assert_eq[String](expected, s)
    else h.fail("unicode BMP failed")
    end

    // Surrogate pair: \uD834\uDD1E = U+1D11E (musical symbol G clef)
    match JSONParser.parse("\"\\uD834\\uDD1E\"")
    | let j: JSONValue =>
      let s = j as String
      let expected = recover val String.from_utf32(0x1D11E) end
      h.assert_eq[String](expected, s)
    else h.fail("surrogate pair failed")
    end

    // Control char via unicode escape: \u001F
    match JSONParser.parse("\"\\u001F\"")
    | let j: JSONValue =>
      let s = j as String
      h.assert_eq[USize](1, s.size())
      h.assert_eq[U8](0x1F, try s(0)? else 0 end)
    else h.fail("control char escape failed")
    end

class \nodoc\ iso _TestParseContainers is UnitTest
  fun name(): String => "json/parse/containers"

  fun apply(h: TestHelper) ? =>
    // Empty object
    match \exhaustive\ JSONParser.parse("{}")
    | let j: JSONValue =>
      let obj = j as JSONObject
      h.assert_eq[USize](0, obj.size())
    else h.fail("empty object failed")
    end

    // Empty array
    match JSONParser.parse("[]")
    | let j: JSONValue =>
      let arr = j as JSONArray
      h.assert_eq[USize](0, arr.size())
    else h.fail("empty array failed")
    end

    // Nested structure
    match JSONParser.parse("""{"a":{"b":[1,2]}}""")
    | let j: JSONValue =>
      let nav = JSONNav(j)
      h.assert_eq[I64](1, nav("a")("b")(USize(0)).as_i64()?)
      h.assert_eq[I64](2, nav("a")("b")(USize(1)).as_i64()?)
    else h.fail("nested structure failed")
    end

    // Whitespace between tokens
    match JSONParser.parse("  { \"a\" :  1  ,  \"b\" :  2  }  ")
    | let j: JSONValue =>
      let obj = j as JSONObject
      h.assert_eq[USize](2, obj.size())
    else h.fail("whitespace handling failed")
    end

class \nodoc\ iso _TestParseWholeDocument is UnitTest
  fun name(): String => "json/parse/whole-document"

  fun apply(h: TestHelper) ? =>
    let src =
      """
      {"store":{"book":[{"title":"A","author":"X","price":10},{"title":"B","author":"Y","price":20}],"bicycle":{"color":"red","price":15}}}
      """
    match \exhaustive\ JSONParser.parse(src)
    | let j: JSONValue =>
      let nav = JSONNav(j)
      h.assert_eq[String]("A", nav("store")("book")(USize(0))("title").as_string()?)
      h.assert_eq[String]("Y", nav("store")("book")(USize(1))("author").as_string()?)
      h.assert_eq[I64](15, nav("store")("bicycle")("price").as_i64()?)
      h.assert_eq[String]("red", nav("store")("bicycle")("color").as_string()?)
    | let e: JSONParseError =>
      h.fail("Whole document parse failed: " + e.string())
    end

// ===================================================================
// Example Tests — Parser Errors
// ===================================================================
class \nodoc\ iso _TestParseErrors is UnitTest
  fun name(): String => "json/parse/errors"

  fun apply(h: TestHelper) =>
    _assert_parse_error(h, "", "empty input")
    _assert_parse_error(h, "hello", "bare word")
    _assert_parse_error(h, """{"a":1,}""", "trailing comma in object")
    _assert_parse_error(h, "[1,]", "trailing comma in array")
    _assert_parse_error(h, """{"a":1""", "unclosed object")
    _assert_parse_error(h, "[1", "unclosed array")
    _assert_parse_error(h, "[1}", "array closed by brace")
    _assert_parse_error(h, """{"a":1]""", "object closed by bracket")
    _assert_parse_error(h, "\"hello", "unterminated string")
    _assert_parse_error(h, "\"\\x\"", "bad escape")
    _assert_parse_error(h, "1 2", "trailing content")
    _assert_parse_error(h, "\"\\u00GG\"", "bad unicode hex")

    // Leading zeros (RFC 8259)
    _assert_parse_error(h, "01", "leading zero")
    _assert_parse_error(h, "007", "leading zeros")
    _assert_parse_error(h, "00", "double zero")
    _assert_parse_error(h, "-01", "negative leading zero")

    // Malformed numbers: a fraction or exponent needs at least one digit, and a
    // sign needs digits after it.
    _assert_parse_error(h, "1.", "trailing dot")
    _assert_parse_error(h, "1.e5", "dot with no fraction digit")
    _assert_parse_error(h, "1e", "exponent with no digit")
    _assert_parse_error(h, "1E", "capital exponent with no digit")
    _assert_parse_error(h, "1e+", "exponent sign with no digit")
    _assert_parse_error(h, "1e-", "negative exponent with no digit")
    _assert_parse_error(h, "1..2", "double dot")
    _assert_parse_error(h, "-", "lone minus")

    // Raw control char (byte < 0x20)
    let ctrl =
      recover val
        String(3)
          .> push('"')
          .> push(0x01)
          .> push('"')
      end
    _assert_parse_error(h, ctrl, "raw control char")

  fun _assert_parse_error(h: TestHelper, input: String, label: String) =>
    match \exhaustive\ JSONParser.parse(input)
    | let _: JSONParseError => None // expected
    | let _: JSONValue => h.fail("Expected error for: " + label)
    end

class \nodoc\ iso _TestParseErrorLoneSurrogates is UnitTest
  fun name(): String => "json/parse/lone-surrogates"

  fun apply(h: TestHelper) =>
    // High surrogate without low
    _assert_parse_error(h, "\"\\uD800\"", "high surrogate alone")

    // Lone low surrogate
    _assert_parse_error(h, "\"\\uDC00\"", "lone low surrogate")

    // High surrogate followed by non-surrogate
    _assert_parse_error(h, "\"\\uD800\\u0041\"", "high + non-surrogate")

  fun _assert_parse_error(h: TestHelper, input: String, label: String) =>
    match \exhaustive\ JSONParser.parse(input)
    | let _: JSONParseError => None // expected
    | let _: JSONValue => h.fail("Expected error for: " + label)
    end

// ===================================================================
// Example Tests — Serialization
// ===================================================================
class \nodoc\ iso _TestPrintCompact is UnitTest
  fun name(): String => "json/print/compact"

  fun apply(h: TestHelper) =>
    // Empty containers
    h.assert_eq[String]("{}", JSONObject.print())
    h.assert_eq[String]("[]", JSONArray.print())

    // Object with entries
    let obj = JSONObject.update("a", I64(1))
    let obj_s: String val = obj.print()
    h.assert_eq[String]("""{"a":1}""", obj_s)

    // Array with entries
    let arr = JSONArray.push(I64(1)).push(I64(2))
    let arr_s: String val = arr.print()
    h.assert_eq[String]("[1,2]", arr_s)

    // Boolean, null via array
    let mixed = JSONArray
      .push(true)
      .push(false)
      .push(None)
    let mixed_s: String val = mixed.print()
    h.assert_eq[String]("[true,false,null]", mixed_s)

    // String with special chars
    let str_arr = JSONArray.push("a\"b\\c\nd")
    let str_s: String val = str_arr.print()
    h.assert_eq[String]("""["a\"b\\c\nd"]""", str_s)

class \nodoc\ iso _TestPrintPretty is UnitTest
  fun name(): String => "json/print/pretty"

  fun apply(h: TestHelper) =>
    // Empty containers stay compact
    h.assert_eq[String]("{}", JSONObject.pretty_print())
    h.assert_eq[String]("[]", JSONArray.pretty_print())

    // Simple object
    let obj = JSONObject.update("a", I64(1))
    let expected = "{\n  \"a\": 1\n}"
    h.assert_eq[String](expected, obj.pretty_print())

    // Nested
    let inner = JSONObject.update("x", I64(42))
    let outer = JSONObject.update("inner", inner)
    let nested_s: String val = outer.pretty_print()
    h.assert_true(nested_s.contains("    \"x\": 42"))

    // Custom indent
    let tab_s: String val = obj.pretty_print("\t")
    h.assert_true(tab_s.contains("\t\"a\": 1"))

    // Array
    let arr = JSONArray.push(I64(1)).push(I64(2))
    let arr_s: String val = arr.pretty_print()
    let arr_expected = "[\n  1,\n  2\n]"
    h.assert_eq[String](arr_expected, arr_s)

class \nodoc\ iso _TestPrintFloats is UnitTest
  fun name(): String => "json/print/floats"

  fun apply(h: TestHelper) =>
    // Whole-number float gets .0 suffix
    let whole = JSONArray.push(F64(1))
    let whole_s: String val = whole.print()
    h.assert_eq[String]("[1.0]", whole_s)

    // Decimal float kept as-is
    let dec = JSONArray.push(F64(1.5))
    let dec_s: String val = dec.print()
    h.assert_eq[String]("[1.5]", dec_s)

    // Negative float
    let neg = JSONArray.push(F64(-3.25))
    let neg_s: String val = neg.print()
    h.assert_eq[String]("[-3.25]", neg_s)

    // Zero
    let zero = JSONArray.push(F64(0))
    let zero_s: String val = zero.print()
    h.assert_eq[String]("[0.0]", zero_s)

    // Precision is preserved: a value needing more than six significant digits
    // keeps all of them. `from_bits` names the exact double, sidestepping the
    // compiler's mis-rounding of some decimal float literals. Each value also
    // drives a specific rung of `_shortest`'s 15-then-16-then-17 search.
    h.assert_eq[String](
      "3.141592653589793",
      JSONPrinter.print(F64.from_bits(0x400921FB54442D18)))    // pi, needs 16
    h.assert_eq[String](
      "0.30000000000000004",
      JSONPrinter.print(F64.from_bits(0x3FD3333333333334)))    // needs 17
    h.assert_eq[String](
      "0.3",
      JSONPrinter.print(F64.from_bits(0x3FD3333333333333)))    // needs 15
    // The 0.3 / 0.30000000000000004 pair are adjacent doubles: the printer
    // must emit enough digits to tell them apart.

    // Extremes of the finite range keep full precision: the smallest positive
    // denormal, then the largest and most-negative finite doubles.
    h.assert_eq[String](
      "4.94065645841247e-324",
      JSONPrinter.print(F64.from_bits(0x0000000000000001)))
    h.assert_eq[String](
      "1.7976931348623157e+308",
      JSONPrinter.print(F64.from_bits(0x7FEFFFFFFFFFFFFF)))
    h.assert_eq[String](
      "-1.7976931348623157e+308",
      JSONPrinter.print(F64.from_bits(0xFFEFFFFFFFFFFFFF)))

    // A value that needs few digits still prints short — not padded to 17.
    h.assert_eq[String](
      "0.1",
      JSONPrinter.print(F64.from_bits(0x3FB999999999999A)))

    // A large whole-number float still gets the `.0` suffix, and negative zero
    // keeps its sign along with the suffix.
    h.assert_eq[String](
      "9007199254740992.0",
      JSONPrinter.print(F64.from_bits(0x4340000000000000)))    // 2^53
    h.assert_eq[String](
      "-0.0",
      JSONPrinter.print(F64.from_bits(0x8000000000000000)))

class \nodoc\ iso _TestPrintFloatTypePreservation is UnitTest
  fun name(): String => "json/print/float-type-preservation"

  fun apply(h: TestHelper) =>
    // A whole-number float keeps a decimal point so it re-parses as F64, not
    // I64 — the reason `_float` appends `.0`.
    h.assert_true(
      match JSONParser.parse(JSONPrinter.print(F64(1)))
      | let _: F64 => true
      else false
      end)
    h.assert_true(
      match JSONParser.parse(
        JSONPrinter.print(F64.from_bits(0x4340000000000000)))
      | let _: F64 => true
      else false
      end)

    // A whole-number float whose shortest form is exponential must NOT get a
    // `.0` (that would be `1e+16.0`, not valid JSON). It still round-trips.
    let exp_whole: String val = JSONPrinter.print(F64(1e16))
    h.assert_true(exp_whole.contains("e"))
    h.assert_false(exp_whole.contains(".0"))
    h.assert_true(
      match JSONParser.parse(exp_whole)
      | let f: F64 => f == F64(1e16)
      else false
      end)

    // Negative zero round-trips as an F64 with its sign bit intact.
    match JSONParser.parse(
      JSONPrinter.print(F64.from_bits(0x8000000000000000)))
    | let f: F64 => h.assert_eq[U64](0x8000000000000000, f.bits())
    else h.fail("negative zero did not round-trip as F64")
    end

    // The finite extremes survive a full print -> parse round-trip with every
    // bit intact. The parser's range check sits right at this boundary, so the
    // printed 17-digit form of the largest magnitudes must not reparse to
    // infinity, and the smallest denormal must not collapse to zero.
    let extremes =
      [ as U64: 0x7FEFFFFFFFFFFFFF; 0xFFEFFFFFFFFFFFFF; 0x0000000000000001 ]
    for bits in extremes.values() do
      match JSONParser.parse(JSONPrinter.print(F64.from_bits(bits)))
      | let f: F64 => h.assert_eq[U64](bits, f.bits())
      else h.fail("extreme did not round-trip as F64: " + bits.string())
      end
    end

class \nodoc\ iso _TestPrintNonFinite is UnitTest
  fun name(): String => "json/print/non-finite"

  fun apply(h: TestHelper) =>
    // A non-finite F64 (infinity or NaN) has no JSON representation, so it
    // serializes as `null` — the printer must always produce valid JSON.
    let inf: F64 = F64(1e308) * F64(10)
    let neg_inf: F64 = -inf
    let nan: F64 = inf - inf

    // Guard: confirm the inputs really are non-finite.
    h.assert_false(inf.finite())
    h.assert_false(neg_inf.finite())
    h.assert_false(nan.finite())

    h.assert_eq[String]("null", JSONPrinter.print(inf))
    h.assert_eq[String]("null", JSONPrinter.print(neg_inf))
    h.assert_eq[String]("null", JSONPrinter.print(nan))

    // Inside a container, compact and pretty.
    h.assert_eq[String](
      """{"v":null}""",
      JSONPrinter.print(JSONObject.update("v", inf)))
    h.assert_eq[String](
      "[null,null]",
      JSONPrinter.print(JSONArray.push(inf).push(nan)))
    h.assert_eq[String](
      "[\n  null\n]",
      JSONPrinter.pretty(JSONArray.push(neg_inf)))

    // The direct print methods on JSONArray/JSONObject route through the same
    // path, so they coerce too.
    h.assert_eq[String]("[null]", JSONArray.push(inf).print())

    // A finite float is unaffected — whole numbers still keep a `.0`.
    h.assert_eq[String]("1.0", JSONPrinter.print(F64(1)))
    h.assert_eq[String]("2.5", JSONPrinter.print(F64(2.5)))

class \nodoc\ iso _TestPrinterPretty is UnitTest
  fun name(): String => "json/printer/pretty"

  fun apply(h: TestHelper) =>
    // Scalars are unaffected by pretty-printing.
    h.assert_eq[String]("null", JSONPrinter.pretty(None))
    h.assert_eq[String]("42", JSONPrinter.pretty(I64(42)))

    // Objects are indented.
    let obj = JSONObject.update("a", I64(1))
    h.assert_eq[String]("{\n  \"a\": 1\n}", JSONPrinter.pretty(obj))

    // Arrays are indented.
    let arr = JSONArray.push(I64(1)).push(I64(2))
    h.assert_eq[String]("[\n  1,\n  2\n]", JSONPrinter.pretty(arr))

    // Nested containers indent one level deeper.
    let nested = JSONObject.update("inner", JSONObject.update("x", I64(42)))
    h.assert_eq[String](
      "{\n  \"inner\": {\n    \"x\": 42\n  }\n}",
      JSONPrinter.pretty(nested))

    // Custom indent string.
    let tab_s: String val = JSONPrinter.pretty(obj, "\t")
    h.assert_true(tab_s.contains("\t\"a\": 1"))

class \nodoc\ iso _TestPrinterScalars is UnitTest
  fun name(): String => "json/printer/scalars"

  fun apply(h: TestHelper) =>
    // JSON null is None — must serialize as `null`, not `None`.
    h.assert_eq[String]("null", JSONPrinter.print(None))

    // Booleans
    h.assert_eq[String]("true", JSONPrinter.print(true))
    h.assert_eq[String]("false", JSONPrinter.print(false))

    // Integers
    h.assert_eq[String]("42", JSONPrinter.print(I64(42)))
    h.assert_eq[String]("-7", JSONPrinter.print(I64(-7)))

    // Floats — whole numbers keep a `.0` so they stay floats on re-parse.
    h.assert_eq[String]("1.0", JSONPrinter.print(F64(1)))
    h.assert_eq[String]("2.5", JSONPrinter.print(F64(2.5)))

    // Bare strings are quoted.
    h.assert_eq[String]("\"hello\"", JSONPrinter.print("hello"))

    // A lone double-quote must be escaped (not copied verbatim).
    h.assert_eq[String]("\"\\\"\"", JSONPrinter.print("\""))

    // Every named control-char escape produces its short form, not \u00xx.
    h.assert_eq[String]("\"\\b\"", JSONPrinter.print("\b"))
    h.assert_eq[String]("\"\\f\"", JSONPrinter.print("\f"))
    h.assert_eq[String]("\"\\n\"", JSONPrinter.print("\n"))
    h.assert_eq[String]("\"\\r\"", JSONPrinter.print("\r"))
    h.assert_eq[String]("\"\\t\"", JSONPrinter.print("\t"))

    // Control chars below 0x20 without a named escape use \u00xx.
    h.assert_eq[String]("\"\\u0001\"", JSONPrinter.print(""))

    // Whole objects and arrays route through the same public entry point,
    // including the empty-container fast paths.
    h.assert_eq[String]("{}", JSONPrinter.print(JSONObject))
    h.assert_eq[String]("[]", JSONPrinter.print(JSONArray))
    h.assert_eq[String](
      """{"a":1}""",
      JSONPrinter.print(JSONObject.update("a", I64(1))))
    h.assert_eq[String](
      "[1,2]",
      JSONPrinter.print(JSONArray.push(I64(1)).push(I64(2))))

// ===================================================================
// Example Tests — Collections
// ===================================================================
class \nodoc\ iso _TestObjectGetOrElse is UnitTest
  fun name(): String => "json/object/get-or-else"

  fun apply(h: TestHelper) ? =>
    let obj = JSONObject.update("key", I64(42))

    // Present key returns stored value
    let got = obj.get_or_else("key", I64(0)) as I64
    h.assert_eq[I64](42, got)

    // Missing key returns default
    let missing = obj.get_or_else("nope", I64(99)) as I64
    h.assert_eq[I64](99, missing)

    // Default can be different type than stored
    let str_default = obj.get_or_else("nope", "default") as String
    h.assert_eq[String]("default", str_default)

class \nodoc\ iso _TestArrayUpdate is UnitTest
  fun name(): String => "json/array/update"

  fun apply(h: TestHelper) ? =>
    let arr = JSONArray.push(I64(1)).push(I64(2)).push(I64(3))

    // Update replaces element
    let updated = arr.update(1, I64(99))?
    h.assert_eq[I64](99, updated(1)? as I64)

    // Original unchanged
    h.assert_eq[I64](2, arr(1)? as I64)

    // Other elements preserved
    h.assert_eq[I64](1, updated(0)? as I64)
    h.assert_eq[I64](3, updated(2)? as I64)

    // Out of bounds raises
    h.assert_error({() ? => arr.update(10, I64(0))? })

// ===================================================================
// Example Tests — Navigation
// ===================================================================
class \nodoc\ iso _TestNavSuccess is UnitTest
  fun name(): String => "json/nav/success"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("name", "Alice")
      .update("age", I64(30))
      .update("score", F64(9.5))
      .update("active", true)
      .update("data", None)
      .update("tags", JSONArray.push("a").push("b"))
      .update("meta", JSONObject.update("x", I64(1)))

    let nav = JSONNav(doc)

    // Object key lookup
    h.assert_eq[String]("Alice", nav("name").as_string()?)

    // Chained navigation
    h.assert_eq[I64](1, nav("meta")("x").as_i64()?)

    // Array index
    h.assert_eq[String]("a", nav("tags")(USize(0)).as_string()?)

    // All terminal extractors
    h.assert_eq[String]("Alice", nav("name").as_string()?)
    h.assert_eq[I64](30, nav("age").as_i64()?)
    h.assert_eq[F64](9.5, nav("score").as_f64()?)
    h.assert_eq[Bool](true, nav("active").as_bool()?)
    nav("data").as_null()?  // should not raise
    nav("meta").as_object()?  // should not raise
    nav("tags").as_array()?  // should not raise

class \nodoc\ iso _TestNavNotFound is UnitTest
  fun name(): String => "json/nav/not-found"

  fun apply(h: TestHelper) =>
    let obj = JSONObject.update("a", I64(1))
    let arr = JSONArray.push(I64(1))
    let nav_obj = JSONNav(obj)
    let nav_arr = JSONNav(arr)

    // Missing key
    h.assert_false(nav_obj("missing").found())

    // Out of bounds index
    h.assert_false(nav_arr(USize(99)).found())

    // Type mismatch: string key on array
    h.assert_false(nav_arr("key").found())

    // Type mismatch: index on object
    h.assert_false(nav_obj(USize(0)).found())

    // JSONNotFound propagates through chain
    h.assert_false(nav_obj("x")("y")("z").found())

    // Extractor on JSONNotFound raises
    h.assert_error({() ? => nav_obj("missing").as_string()? })

class \nodoc\ iso _TestNavInspection is UnitTest
  fun name(): String => "json/nav/inspection"

  fun apply(h: TestHelper) ? =>
    let obj = JSONObject.update("a", I64(1)).update("b", I64(2))
    let arr = JSONArray.push(I64(1)).push(I64(2)).push(I64(3))

    // found()
    h.assert_true(JSONNav(obj).found())
    h.assert_false(JSONNav(obj)("missing").found())

    // size() on object and array
    h.assert_eq[USize](2, JSONNav(obj).size()?)
    h.assert_eq[USize](3, JSONNav(arr).size()?)

    // size() raises on non-container
    h.assert_error({() ? => JSONNav(I64(1)).size()? })

    // json() returns raw value
    match JSONNav(obj).json()
    | let o: JSONObject => h.assert_eq[USize](2, o.size())
    else h.fail("json() returned wrong type")
    end

    match JSONNav(obj)("missing").json()
    | JSONNotFound => None // expected
    else h.fail("Expected JSONNotFound from json()")
    end

// ===================================================================
// Example Tests — Lens
// ===================================================================
class \nodoc\ iso _TestLensGet is UnitTest
  fun name(): String => "json/lens/get"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("a", JSONObject.update("b", I64(42)))

    // Identity lens returns root
    match JSONLens.get(doc)
    | let j: JSONValue =>
      let obj = j as JSONObject
      h.assert_true(obj.contains("a"))
    else h.fail("Identity get failed")
    end

    // Nested path
    let lens = JSONLens("a")("b")
    match lens.get(doc)
    | let j: JSONValue => h.assert_eq[I64](42, j as I64)
    else h.fail("Nested get failed")
    end

    // Missing intermediate -> JSONNotFound
    let missing = JSONLens("x")("y")
    match \exhaustive\ missing.get(doc)
    | JSONNotFound => None
    else h.fail("Expected JSONNotFound for missing path")
    end

    // Type mismatch -> JSONNotFound
    let mismatch = JSONLens("a")("b")("c")
    match \exhaustive\ mismatch.get(doc)
    | JSONNotFound => None
    else h.fail("Expected JSONNotFound for type mismatch")
    end

class \nodoc\ iso _TestLensSet is UnitTest
  fun name(): String => "json/lens/set"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "a",
        JSONObject
          .update("b", I64(1))
          .update("c", I64(2)))

    // Identity lens replaces root
    match JSONLens.set(doc, I64(99))
    | let j: JSONValue => h.assert_eq[I64](99, j as I64)
    else h.fail("Identity set failed")
    end

    // Nested set
    let lens = JSONLens("a")("b")
    match lens.set(doc, I64(42))
    | let j: JSONValue =>
      let nav = JSONNav(j)
      h.assert_eq[I64](42, nav("a")("b").as_i64()?)
      // Sibling preserved
      h.assert_eq[I64](2, nav("a")("c").as_i64()?)
    else h.fail("Nested set failed")
    end

    // Original unchanged
    let nav = JSONNav(doc)
    h.assert_eq[I64](1, nav("a")("b").as_i64()?)

    // Missing intermediate -> JSONNotFound
    let missing = JSONLens("x")("y")
    match \exhaustive\ missing.set(doc, I64(1))
    | JSONNotFound => None
    else h.fail("Expected JSONNotFound for missing intermediate")
    end

class \nodoc\ iso _TestLensRemove is UnitTest
  fun name(): String => "json/lens/remove"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "a",
        JSONObject
          .update("b", I64(1))
          .update("c", I64(2)))

    // Remove key
    let lens = JSONLens("a")("b")
    match lens.remove(doc)
    | let j: JSONValue =>
      let nav = JSONNav(j)
      h.assert_false(nav("a")("b").found())
      // Sibling preserved
      h.assert_eq[I64](2, nav("a")("c").as_i64()?)
    else h.fail("Remove failed")
    end

    // Remove on array index -> JSONNotFound
    let arr_doc = JSONObject.update("arr", JSONArray.push(I64(1)))
    let arr_lens = JSONLens("arr")(USize(0))
    match \exhaustive\ arr_lens.remove(arr_doc)
    | JSONNotFound => None
    else h.fail("Expected JSONNotFound for array index remove")
    end

class \nodoc\ iso _TestLensComposition is UnitTest
  fun name(): String => "json/lens/composition"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "a",
        JSONObject
          .update(
            "b",
            JSONObject
              .update("c", I64(99))))

    // compose equivalent to chained apply
    let lens_ab = JSONLens("a")("b")
    let lens_c = JSONLens("c")
    let composed = lens_ab.compose(lens_c)
    let chained = JSONLens("a")("b")("c")

    match composed.get(doc)
    | let j1: JSONValue =>
      match chained.get(doc)
      | let j2: JSONValue =>
        h.assert_eq[I64](j1 as I64, j2 as I64)
      else h.fail("Chained get failed")
      end
    else h.fail("Composed get failed")
    end

    // or_else falls back when first lens fails
    let missing = JSONLens("x")
    let found = JSONLens("a")("b")("c")
    let fallback = missing.or_else(found)
    match fallback.get(doc)
    | let j: JSONValue => h.assert_eq[I64](99, j as I64)
    else h.fail("or_else fallback failed")
    end

    // or_else uses first when it succeeds
    let first_wins = found.or_else(missing)
    match \exhaustive\ first_wins.get(doc)
    | let j: JSONValue => h.assert_eq[I64](99, j as I64)
    else h.fail("or_else first-match failed")
    end

    // Composed set modifies deeply nested value
    match composed.set(doc, I64(0))
    | let j: JSONValue =>
      let nav = JSONNav(j)
      h.assert_eq[I64](0, nav("a")("b")("c").as_i64()?)
    else h.fail("Composed set failed")
    end

// ===================================================================
// Example Tests — JSONPath
// ===================================================================
class \nodoc\ iso _TestJSONPathParse is UnitTest
  fun name(): String => "json/jsonpath/parse"

  fun apply(h: TestHelper) =>
    // All valid expressions should parse
    let valid: Array[String] val =
      [
        "$"
        "$.name"
        "$['name']"
        """$["name"]"""
        "$[0]"
        "$[-1]"
        "$.*"
        "$[*]"
        "$..name"
        "$..*"
        "$[0:2]"
        "$[:2]"
        "$[1:]"
        "$[0,1,2]"
        "$.store.book[*].author"
        "$[0:2:1]"
        "$[::2]"
        "$[::-1]"
        "$[1:4:2]"
        "$[::0]"
      ]
    for path_str in valid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPath => None // pass
      | let e: JSONPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // compile raises on bad input
    h.assert_error({() ? => JSONPathParser.compile("invalid")? })

    // compile succeeds on good input
    try
      JSONPathParser.compile("$.a")?
    else
      h.fail("compile should succeed for $.a")
    end

class \nodoc\ iso _TestJSONPathParseErrors is UnitTest
  fun name(): String => "json/jsonpath/parse-errors"

  fun apply(h: TestHelper) =>
    let invalid: Array[String] val =
      [
        ""         // empty string
        "name"     // missing $
        "$!"       // bad segment char
        "$[0"      // unclosed bracket
        "$['open"  // unterminated string
      ]
    for path_str in invalid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPathParseError => None // expected
      | let _: JSONPath =>
        h.fail("Expected error for: " + path_str)
      end
    end

class \nodoc\ iso _TestJSONPathQueryBasic is UnitTest
  fun name(): String => "json/jsonpath/query/basic"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("a", I64(1))
      .update("b", JSONObject.update("c", I64(2)))

    let arr_doc = JSONArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))

    // Dot child
    let p1 = JSONPathParser.compile("$.a")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())
    h.assert_eq[I64](1, r1(0)? as I64)

    // Nested dots
    let p2 = JSONPathParser.compile("$.b.c")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())
    h.assert_eq[I64](2, r2(0)? as I64)

    // Index
    let p3 = JSONPathParser.compile("$[0]")?
    let r3 = p3.query(arr_doc)
    h.assert_eq[USize](1, r3.size())
    h.assert_eq[I64](10, r3(0)? as I64)

    // Negative index
    let p4 = JSONPathParser.compile("$[-1]")?
    let r4 = p4.query(arr_doc)
    h.assert_eq[USize](1, r4.size())
    h.assert_eq[I64](30, r4(0)? as I64)

    // Missing key -> empty
    let p5 = JSONPathParser.compile("$.missing")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](0, r5.size())

    // Type mismatch -> empty
    let p6 = JSONPathParser.compile("$.a")?
    let r6 = p6.query(arr_doc)
    h.assert_eq[USize](0, r6.size())

    // query_one returns first
    match p1.query_one(doc)
    | let j: JSONValue => h.assert_eq[I64](1, j as I64)
    else h.fail("query_one should find $.a")
    end

    // query_one returns JSONNotFound when empty
    match \exhaustive\ p5.query_one(doc)
    | JSONNotFound => None
    else h.fail("query_one should return JSONNotFound for missing")
    end

class \nodoc\ iso _TestJSONPathQueryAdvanced is UnitTest
  fun name(): String => "json/jsonpath/query/advanced"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("a", I64(1))
      .update("b", I64(2))
      .update("c", JSONObject.update("a", I64(3)))

    let arr = JSONArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))
      .push(I64(40))

    // Wildcard on object
    let p1 = JSONPathParser.compile("$.*")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](3, r1.size())

    // Wildcard on array
    let p2 = JSONPathParser.compile("$[*]")?
    let r2 = p2.query(arr)
    h.assert_eq[USize](4, r2.size())

    // Recursive descent
    let p3 = JSONPathParser.compile("$..a")?
    let r3 = p3.query(doc)
    // Should find doc.a (1) and doc.c.a (3)
    h.assert_eq[USize](2, r3.size())

    // Slice [0:2]
    let p4 = JSONPathParser.compile("$[0:2]")?
    let r4 = p4.query(arr)
    h.assert_eq[USize](2, r4.size())
    h.assert_eq[I64](10, r4(0)? as I64)
    h.assert_eq[I64](20, r4(1)? as I64)

    // Open-ended slices
    let p5 = JSONPathParser.compile("$[:2]")?
    let r5 = p5.query(arr)
    h.assert_eq[USize](2, r5.size())

    let p6 = JSONPathParser.compile("$[1:]")?
    let r6 = p6.query(arr)
    h.assert_eq[USize](3, r6.size())
    h.assert_eq[I64](20, r6(0)? as I64)

    // Negative slice
    let p7 = JSONPathParser.compile("$[-2:]")?
    let r7 = p7.query(arr)
    h.assert_eq[USize](2, r7.size())
    h.assert_eq[I64](30, r7(0)? as I64)
    h.assert_eq[I64](40, r7(1)? as I64)

    // Union
    let p8 = JSONPathParser.compile("$[0,2]")?
    let r8 = p8.query(arr)
    h.assert_eq[USize](2, r8.size())
    h.assert_eq[I64](10, r8(0)? as I64)
    h.assert_eq[I64](30, r8(1)? as I64)

    // Descendant wildcard
    let p9 = JSONPathParser.compile("$..*")?
    let r9 = p9.query(doc)
    // Should include all values at all levels
    h.assert_true(r9.size() > 0)

class \nodoc\ iso _TestJSONPathQueryComplex is UnitTest
  fun name(): String => "json/jsonpath/query/complex"

  fun apply(h: TestHelper) ? =>
    let book1 = JSONObject
      .update("title", "A")
      .update("author", "X")
      .update("price", I64(10))

    let book2 = JSONObject
      .update("title", "B")
      .update("author", "Y")
      .update("price", I64(20))

    let bicycle = JSONObject
      .update("color", "red")
      .update("price", I64(15))

    let store = JSONObject
      .update("book", JSONArray.push(book1).push(book2))
      .update("bicycle", bicycle)

    let doc = JSONObject.update("store", store)

    // All book authors
    let p1 = JSONPathParser.compile("$.store.book[*].author")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // All prices (recursive descent)
    let p2 = JSONPathParser.compile("$.store..price")?
    let r2 = p2.query(doc)
    // 2 book prices + 1 bicycle price = 3
    h.assert_eq[USize](3, r2.size())

    // First book title
    let p3 = JSONPathParser.compile("$.store.book[0].title")?
    match p3.query_one(doc)
    | let j: JSONValue => h.assert_eq[String]("A", j as String)
    else h.fail("Should find first book title")
    end

class \nodoc\ iso _TestJSONPathQuerySliceStep is UnitTest
  fun name(): String => "json/jsonpath/query/slice-step"

  fun apply(h: TestHelper) ? =>
    let arr = JSONArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))
      .push(I64(40))
      .push(I64(50))

    // Positive step: every other element [0:5:2] -> [10, 30, 50]
    let p1 = JSONPathParser.compile("$[0:5:2]")?
    let r1 = p1.query(arr)
    h.assert_eq[USize](3, r1.size())
    h.assert_eq[I64](10, r1(0)? as I64)
    h.assert_eq[I64](30, r1(1)? as I64)
    h.assert_eq[I64](50, r1(2)? as I64)

    // Step=1 explicit same as omitted [1:4:1] -> [20, 30, 40]
    let p2 = JSONPathParser.compile("$[1:4:1]")?
    let r2 = p2.query(arr)
    h.assert_eq[USize](3, r2.size())
    h.assert_eq[I64](20, r2(0)? as I64)

    // Negative step: reverse [4:1:-1] -> [50, 40, 30]
    let p3 = JSONPathParser.compile("$[4:1:-1]")?
    let r3 = p3.query(arr)
    h.assert_eq[USize](3, r3.size())
    h.assert_eq[I64](50, r3(0)? as I64)
    h.assert_eq[I64](40, r3(1)? as I64)
    h.assert_eq[I64](30, r3(2)? as I64)

    // Negative step with defaults: reverse entire array [::-1]
    let p4 = JSONPathParser.compile("$[::-1]")?
    let r4 = p4.query(arr)
    h.assert_eq[USize](5, r4.size())
    h.assert_eq[I64](50, r4(0)? as I64)
    h.assert_eq[I64](10, r4(4)? as I64)

    // Step=0 produces no results
    let p5 = JSONPathParser.compile("$[::0]")?
    let r5 = p5.query(arr)
    h.assert_eq[USize](0, r5.size())

    // Negative indices with step: [-4:-1:2] -> [20, 40]
    let p6 = JSONPathParser.compile("$[-4:-1:2]")?
    let r6 = p6.query(arr)
    h.assert_eq[USize](2, r6.size())
    h.assert_eq[I64](20, r6(0)? as I64)
    h.assert_eq[I64](40, r6(1)? as I64)

    // Step with open start/end: [::2] -> [10, 30, 50]
    let p7 = JSONPathParser.compile("$[::2]")?
    let r7 = p7.query(arr)
    h.assert_eq[USize](3, r7.size())
    h.assert_eq[I64](10, r7(0)? as I64)
    h.assert_eq[I64](30, r7(1)? as I64)
    h.assert_eq[I64](50, r7(2)? as I64)

    // Negative step, open start/end: [::-2] -> [50, 30, 10]
    let p8 = JSONPathParser.compile("$[::-2]")?
    let r8 = p8.query(arr)
    h.assert_eq[USize](3, r8.size())
    h.assert_eq[I64](50, r8(0)? as I64)
    h.assert_eq[I64](30, r8(1)? as I64)
    h.assert_eq[I64](10, r8(2)? as I64)

    // Wrong direction: positive step, start > end -> empty
    let p9 = JSONPathParser.compile("$[3:1:1]")?
    let r9 = p9.query(arr)
    h.assert_eq[USize](0, r9.size())

    // Wrong direction: negative step, start < end -> empty
    let p10 = JSONPathParser.compile("$[1:3:-1]")?
    let r10 = p10.query(arr)
    h.assert_eq[USize](0, r10.size())

    // Empty array
    let empty = JSONArray
    let p11 = JSONPathParser.compile("$[::2]")?
    let r11 = p11.query(empty)
    h.assert_eq[USize](0, r11.size())

    // Slice on non-array produces empty result
    let obj = JSONObject.update("a", I64(1))
    let p12 = JSONPathParser.compile("$[::2]")?
    let r12 = p12.query(obj)
    h.assert_eq[USize](0, r12.size())

// ===================================================================
// Example Tests — Token Parser
// ===================================================================
class \nodoc\ iso _TestTokenParserAbort is UnitTest
  fun name(): String => "json/tokenparser/abort"

  fun apply(h: TestHelper) =>
    let parser =
      JSONTokenParser(
        object is JSONTokenNotify
          var _count: USize = 0
          fun ref apply(parser': JSONTokenParser, token: JSONToken) =>
            _count = _count + 1
            if _count >= 2 then
              parser'.abort()
            end
        end)
    // parse should raise because abort() was called mid-document
    var raised = false
    try
      parser.feed("[1,2,3]")?
    else
      raised = true
    end
    h.assert_true(raised)

// ===================================================================
// Property Tests — JSONPath Filter Safety
// ===================================================================
class \nodoc\ iso _FilterSafetyProperty is Property1[String]
  fun name(): String => "json/jsonpath/filter/safety"

  fun gen(): Generator[String] =>
    _JSONValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    match \exhaustive\ JSONParser.parse(sample)
    | let doc: JSONValue =>
      let paths: Array[String] val =
        [
          "$[?@.a]"
          "$[?@.a == 1]"
          "$[?@.a != null]"
          "$[?@.a > 0]"
          "$[?@.a < 100]"
          "$[?@.a >= 0 && @.b <= 10]"
          "$[?@.a == 1 || @.b == 2]"
          "$[?!@.missing]"
          "$[?@.a == $.b]"
          """$[?@.name == "test"]"""
          "$[?@.x == true]"
          "$[?@.x == false]"
          "$[?(@.a > 1)]"
          "$[?length(@.a) > 0]"
          "$[?count(@.*) > 0]"
          """$[?match(@.a, "[a-z]")]"""
          """$[?search(@.a, "test")]"""
          "$[?value(@.a) == 1]"
          """$[?!match(@.a, "x")]"""
        ]
      for path_str in paths.values() do
        try
          let path = JSONPathParser.compile(path_str)?
          let results = path.query(doc)
          ph.assert_true(results.size() >= 0)
        else
          ph.fail("Failed to compile: " + path_str)
        end
      end
    | let _: JSONParseError => None
    end

// ===================================================================
// Property Tests — JSONPath Function Extensions
// ===================================================================
primitive \nodoc\ _SafeIRegexpGen
  """
  Generates valid I-Regexp patterns that are safe to embed in JSONPath
  single-quoted strings. Avoids backslash escapes (\p, \n, etc.) since
  the JSONPath string parser interprets backslashes before the I-Regexp
  parser sees them.
  """
  fun apply(max_depth: USize = 2): Generator[String] =>
    let that = this
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String =>
          that._gen(rnd, max_depth)
      end)

  fun _gen(rnd: Randomness, depth: USize): String =>
    if depth == 0 then return _gen_atom(rnd) end
    match rnd.usize(0, 5)
    | 0 => _gen_atom(rnd)
    | 1 => _gen(rnd, depth - 1) + "|" + _gen(rnd, depth - 1)
    | 2 => _gen_atom(rnd) + _gen_atom(rnd)
    | 3 => "(" + _gen(rnd, depth - 1) + ")" + _gen_quant(rnd)
    | 4 => _gen_atom(rnd) + _gen_quant(rnd)
    | 5 => _gen(rnd, depth - 1) + _gen_atom(rnd)
    else _gen_atom(rnd)
    end

  fun _gen_atom(rnd: Randomness): String =>
    match rnd.usize(0, 3)
    | 0 => String.from_array([rnd.u8('a', 'z')])
    | 1 => "."
    | 2 => "[a-z]"
    | 3 => "[0-9]"
    else "a"
    end

  fun _gen_quant(rnd: Randomness): String =>
    match rnd.usize(0, 3)
    | 0 => ""
    | 1 => "*"
    | 2 => "+"
    | 3 => "?"
    else ""
    end

class \nodoc\ iso _FunctionMatchImpliesSearchProperty
  is Property1[(String, String)]
  """
  If match(@.v, pattern) selects a node, search(@.v, pattern) must also
  select it. Full-string match is a special case of substring search.
  """
  fun name(): String => "json/jsonpath/filter/function/match-implies-search"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      _JSONValueStringGen(2),
      _SafeIRegexpGen(1))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json_str, let pattern) = sample
    match \exhaustive\ JSONParser.parse(json_str)
    | let doc: JSONValue =>
      let match_path: String val = "$[?match(@.v, '" + pattern + "')]"
      let search_path: String val = "$[?search(@.v, '" + pattern + "')]"
      match (JSONPathParser.parse(match_path),
        JSONPathParser.parse(search_path))
      | (let mp: JSONPath, let sp: JSONPath) =>
        let match_results = mp.query(doc)
        let search_results = sp.query(doc)
        // Every match result must also be a search result
        ph.assert_true(
          match_results.size() <= search_results.size(),
          "match returned " + match_results.size().string() +
            " but search returned " +
            search_results.size().string() +
            " for pattern '" + pattern + "'")
      end
    | let _: JSONParseError => None
    end

class \nodoc\ iso _FunctionCountLengthEquivalenceProperty
  is Property1[(String, String)]
  """
  For array values, count(@[*]) must equal length(@). These are two
  independent code paths that should agree on array cardinality.

  Generates two JSON values, wraps each in an array so the "v" field
  is always an array, then asserts count(@.v[*]) == length(@.v) for
  both elements.
  """
  fun name(): String =>
    "json/jsonpath/filter/function/count-length-equivalence"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      _JSONValueStringGen(2),
      _JSONValueStringGen(2))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json1, let json2) = sample
    // Wrap each value inside an array so @.v is always an array.
    // This ensures count(@.v[*]) and length(@.v) both return integers
    // and must agree.
    let wrapped: String val =
      "[{\"v\":[" + json1 + "]},{\"v\":[" + json2 + "]}]"
    match \exhaustive\ JSONParser.parse(wrapped)
    | let doc: JSONValue =>
      match JSONPathParser.parse("$[?count(@.v[*]) == length(@.v)]")
      | let eq_p: JSONPath =>
        let eq_results = eq_p.query(doc)
        // Both elements have array "v", so count and length must agree
        // for both → eq should return 2 results
        ph.assert_eq[USize](
          2,
          eq_results.size(),
          "count(@.v[*]) should equal length(@.v) for arrays")
      end
    | let _: JSONParseError => None
    end

class \nodoc\ iso _FunctionSafetyProperty
  is Property1[(String, String)]
  """
  Function extension paths with generated I-Regexp patterns must never
  crash, regardless of the JSON document or pattern content.
  """
  fun name(): String => "json/jsonpath/filter/function/safety"

  fun gen(): Generator[(String, String)] =>
    Generators.zip2[String, String](
      _JSONValueStringGen(2),
      _SafeIRegexpGen(1))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json_str, let pattern) = sample
    match \exhaustive\ JSONParser.parse(json_str)
    | let doc: JSONValue =>
      let match_path: String val = "$[?match(@.v, '" + pattern + "')]"
      let search_path: String val = "$[?search(@.v, '" + pattern + "')]"
      let not_match: String val = "$[?!match(@.v, '" + pattern + "')]"
      let not_search: String val = "$[?!search(@.v, '" + pattern + "')]"
      let paths: Array[String val] val =
        [
          match_path
          search_path
          not_match
          not_search
          "$[?length(@.v) > 0]"
          "$[?count(@.*) >= 0]"
          "$[?value(@.v) == 1]"
          "$[?length(value(@.v)) > 0]"
          "$[?count(@.v[*]) == length(@.v)]"
        ]
      for path_str in paths.values() do
        match \exhaustive\ JSONPathParser.parse(path_str)
        | let path: JSONPath =>
          let results = path.query(doc)
          ph.assert_true(results.size() >= 0)
        | let e: JSONPathParseError =>
          ph.fail("Failed to compile: " + path_str + " — " + e.string())
        end
      end
    | let _: JSONParseError => None
    end

// ===================================================================
// Example Tests — JSONPath Filter Expressions
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterParse is UnitTest
  fun name(): String => "json/jsonpath/filter/parse"

  fun apply(h: TestHelper) =>
    // Valid filter expressions should parse
    let valid: Array[String] val =
      [
        "$[?@.a]"
        "$[?@.a == 1]"
        """$[?@.a != 'hello']"""
        "$[?@.a > 1.5]"
        "$[?@.a < 10]"
        "$[?@.a <= 10]"
        "$[?@.a >= 0]"
        "$[?@.a == true]"
        "$[?@.a == false]"
        "$[?@.a == null]"
        "$[?!@.a]"
        "$[?@.a && @.b]"
        "$[?@.a || @.b]"
        "$[?(@.a)]"
        "$[?@.a == $.b]"
        "$[?@['key'] > 1]"
        "$[?@[0] == 1]"
        "$[?@.a > 1 && @.b < 10]"
        "$[?@.a == 1 || @.b == 2]"
        "$[?!(@.a && @.b)]"
        "$[?@.a[0].b == 1]"
        "$[?10 > @.price]"
        """$[?"book" == @.type]"""
      ]
    for path_str in valid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPath => None // pass
      | let e: JSONPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // Invalid filter expressions
    let invalid: Array[String] val =
      [
        "$[?]"          // empty filter
        "$[?@[*] == 1]" // wildcard in comparison (not singular)
        "$[?@..a == 1]" // descendant in comparison (not singular)
      ]
    for path_str in invalid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPathParseError => None // expected
      | let _: JSONPath =>
        h.fail("Expected error for: " + path_str)
      end
    end

class \nodoc\ iso _TestJSONPathFilterExistence is UnitTest
  fun name(): String => "json/jsonpath/filter/existence"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "items",
        JSONArray
          .push(JSONObject.update("a", I64(1)))
          .push(JSONObject.update("b", I64(2)))
          .push(JSONObject
            .update("a", I64(3))
            .update("b", I64(4))))

    // @.a exists
    let p1 = JSONPathParser.compile("$.items[?@.a]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // @.b exists
    let p2 = JSONPathParser.compile("$.items[?@.b]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](2, r2.size())

    // Negated existence: !@.a
    let p3 = JSONPathParser.compile("$.items[?!@.a]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

    // No items have key "c"
    let p4 = JSONPathParser.compile("$.items[?@.c]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](0, r4.size())

    // Existence with null values — null value still exists
    let doc2 = JSONObject
      .update(
        "items",
        JSONArray
          .push(JSONObject.update("a", None))
          .push(JSONObject.update("b", I64(1))))
    let p5 = JSONPathParser.compile("$.items[?@.a]")?
    let r5 = p5.query(doc2)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJSONPathFilterComparison is UnitTest
  fun name(): String => "json/jsonpath/filter/comparison"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "store",
        JSONObject
          .update(
            "book",
            JSONArray
              .push(JSONObject.update("title", "A").update("price", I64(8)))
              .push(JSONObject.update("title", "B").update("price", I64(12)))
              .push(JSONObject.update("title", "C").update("price", I64(5)))))

    // Less than
    let p1 = JSONPathParser.compile("$.store.book[?@.price < 10]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // Equal
    let p2 = JSONPathParser.compile("$.store.book[?@.price == 12]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    // Not equal
    let p3 = JSONPathParser.compile("$.store.book[?@.price != 12]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

    // Greater than or equal
    let p4 = JSONPathParser.compile("$.store.book[?@.price >= 8]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](2, r4.size())

    // Greater than
    let p5 = JSONPathParser.compile("$.store.book[?@.price > 8]")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](1, r5.size())

    // Less than or equal
    let p6 = JSONPathParser.compile("$.store.book[?@.price <= 5]")?
    let r6 = p6.query(doc)
    h.assert_eq[USize](1, r6.size())

    // String comparison
    let p7 =
      JSONPathParser.compile(
        """$.store.book[?@.title == "A"]""")?
    let r7 = p7.query(doc)
    h.assert_eq[USize](1, r7.size())

    // Literal on left side
    let p8 = JSONPathParser.compile("$.store.book[?10 > @.price]")?
    let r8 = p8.query(doc)
    h.assert_eq[USize](2, r8.size())

class \nodoc\ iso _TestJSONPathFilterLogical is UnitTest
  fun name(): String => "json/jsonpath/filter/logical"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject.update("a", I64(1)).update("b", I64(2)))
      .push(JSONObject.update("a", I64(3)).update("b", I64(4)))
      .push(JSONObject.update("a", I64(5)).update("b", I64(6)))

    // AND
    let p1 = JSONPathParser.compile("$[?@.a > 1 && @.b < 6]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // OR
    let p2 = JSONPathParser.compile("$[?@.a == 1 || @.a == 5]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](2, r2.size())

    // NOT with parens
    let p3 = JSONPathParser.compile("$[?!(@.a > 3)]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

    // Precedence: && binds tighter than ||
    // @.a == 1 || (@.b == 4 && @.a == 3) -> first and second
    let doc2 = JSONArray
      .push(JSONObject
        .update("a", I64(1)).update("b", I64(2)).update("c", I64(3)))
      .push(JSONObject
        .update("a", I64(4)).update("b", I64(5)).update("c", I64(6)))
      .push(JSONObject
        .update("a", I64(7)).update("b", I64(8)).update("c", I64(9)))

    let p4 =
      JSONPathParser.compile(
        "$[?@.a == 1 || @.b == 5 && @.c == 6]")?
    let r4 = p4.query(doc2)
    // Parsed as: @.a == 1 || (@.b == 5 && @.c == 6)
    h.assert_eq[USize](2, r4.size())

    // Explicit parens override precedence
    let p5 =
      JSONPathParser.compile(
        "$[?(@.a == 1 || @.b == 5) && @.c == 6]")?
    let r5 = p5.query(doc2)
    // Only second matches (b==5, c==6)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJSONPathFilterAbsoluteQuery is UnitTest
  fun name(): String => "json/jsonpath/filter/absolute-query"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("default", "X")
      .update(
        "items",
        JSONArray
          .push(JSONObject.update("type", "X").update("name", "a"))
          .push(JSONObject.update("type", "Y").update("name", "b"))
          .push(JSONObject.update("type", "X").update("name", "c")))

    let p = JSONPathParser.compile("$.items[?@.type == $.default]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

class \nodoc\ iso _TestJSONPathFilterNothing is UnitTest
  fun name(): String => "json/jsonpath/filter/nothing"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject.update("a", I64(1)))
      .push(JSONObject.update("b", I64(2)))
      .push(JSONObject.update("a", None))

    // @.a == 1: only first (Nothing != 1, null != 1)
    let p1 = JSONPathParser.compile("$[?@.a == 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // @.a == null: only third (actual null, not Nothing)
    let p2 = JSONPathParser.compile("$[?@.a == null]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    // @.a != 1: second (Nothing != 1 is true) and third (null != 1 is true)
    let p3 = JSONPathParser.compile("$[?@.a != 1]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

class \nodoc\ iso _TestJSONPathFilterTypes is UnitTest
  fun name(): String => "json/jsonpath/filter/types"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject.update("v", I64(1)))
      .push(JSONObject.update("v", "1"))
      .push(JSONObject.update("v", true))
      .push(JSONObject.update("v", None))

    // No type coercion: 1 != "1"
    let p1 = JSONPathParser.compile("$[?@.v == 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    let p2 = JSONPathParser.compile("""$[?@.v == "1"]""")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    let p3 = JSONPathParser.compile("$[?@.v == true]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

    let p4 = JSONPathParser.compile("$[?@.v == null]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](1, r4.size())

    // String "1" < 2 is false (cross-type)
    let p5 = JSONPathParser.compile("$[?@.v < 2]")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJSONPathFilterDeepEquality is UnitTest
  fun name(): String => "json/jsonpath/filter/deep-equality"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update("target", JSONArray.push(I64(1)).push(I64(2)))
      .update(
        "items",
        JSONArray
          .push(JSONObject
            .update("v", JSONArray.push(I64(1)).push(I64(2))))
          .push(JSONObject
            .update("v", JSONArray.push(I64(1)).push(I64(3))))
          .push(JSONObject
            .update("v", JSONArray.push(I64(1)).push(I64(2)))))

    let p = JSONPathParser.compile("$.items[?@.v == $.target]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

    // Object equality: only an object with the same keys and equal values
    // matches. The three unequal cases each hit a different rejection path in
    // the iterative deep-equality walk: an unequal value; a smaller object
    // (caught by the size check); and a same-size object whose key is absent in
    // the target (caught by the key lookup).
    let odoc = JSONObject
      .update(
        "target",
        JSONObject.update("a", I64(1)).update("b", I64(2)))
      .update(
        "items",
        JSONArray
          .push(JSONObject.update(
            "v",
            JSONObject.update("a", I64(1)).update("b", I64(2))))
          .push(JSONObject.update(
            "v",
            JSONObject.update("a", I64(1)).update("b", I64(9))))
          .push(JSONObject.update(
            "v",
            JSONObject.update("a", I64(1))))
          .push(JSONObject.update(
            "v",
            JSONObject.update("a", I64(1)).update("c", I64(2)))))
    let op = JSONPathParser.compile("$.items[?@.v == $.target]")?
    let or' = op.query(odoc)
    h.assert_eq[USize](1, or'.size())

class \nodoc\ iso _TestJSONPathFilterNumbers is UnitTest
  fun name(): String => "json/jsonpath/filter/numbers"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject.update("v", F64(1.5)))
      .push(JSONObject.update("v", F64(2.5)))
      .push(JSONObject.update("v", F64(3.0)))

    // Float comparison
    let p1 = JSONPathParser.compile("$[?@.v > 2.0]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // Mixed I64/F64: I64(2) == F64(2.0)
    let doc2 = JSONArray
      .push(JSONObject.update("v", I64(2)))
      .push(JSONObject.update("v", I64(3)))

    let p2 = JSONPathParser.compile("$[?@.v == 2.0]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size())

    // Float literal in filter
    let p3 = JSONPathParser.compile("$[?@.v == 3.0]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

class \nodoc\ iso _TestJSONPathFilterOnObjects is UnitTest
  fun name(): String => "json/jsonpath/filter/on-objects"

  fun apply(h: TestHelper) ? =>
    let doc = JSONObject
      .update(
        "data",
        JSONObject
          .update("x", JSONObject.update("active", true))
          .update("y", JSONObject.update("active", false))
          .update("z", JSONObject.update("active", true)))

    let p = JSONPathParser.compile("$.data[?@.active == true]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

class \nodoc\ iso _TestJSONPathFilterNested is UnitTest
  fun name(): String => "json/jsonpath/filter/nested"

  fun apply(h: TestHelper) ? =>
    // Nested filter: outer filter with inner bracket access
    let doc = JSONArray
      .push(JSONObject
        .update("items", JSONArray.push(I64(1)).push(I64(5))))
      .push(JSONObject
        .update("items", JSONArray.push(I64(10)).push(I64(20))))

    // Select elements where items[0] > 5
    let p1 = JSONPathParser.compile("$[?@.items[0] > 5]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // $ reference inside filter resolves to document root
    let doc2 = JSONObject
      .update("threshold", I64(3))
      .update(
        "items",
        JSONArray
          .push(JSONObject.update("v", I64(1)))
          .push(JSONObject.update("v", I64(5)))
          .push(JSONObject.update("v", I64(3))))

    let p2 = JSONPathParser.compile("$.items[?@.v > $.threshold]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size())

// ===================================================================
// Example Tests — JSONPath Function Extension Parse
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterFunctionParse is UnitTest
  fun name(): String => "json/jsonpath/filter/function/parse"

  fun apply(h: TestHelper) =>
    // Valid function expressions
    let valid: Array[String] val =
      [
        """$[?match(@.b, "[jk]")]"""
        """$[?search(@.b, "[jk]")]"""
        "$[?length(@.a) > 3]"
        "$[?count(@.items[*]) > 0]"
        "$[?value(@.items[0]) == 1]"
        """$[?!match(@.b, "[jk]")]"""
        """$[?!search(@.b, "x")]"""
        "$[?length(value(@.items)) > 0]"
        """$[?length(@.a) >= 3 && match(@.b, "x")]"""
        "$[?3 < length(@.a)]"
        "$[?count(@[*]) == length(@)]"
      ]
    for path_str in valid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPath => None
      | let e: JSONPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // Invalid function expressions
    let invalid: Array[(String, String)] val =
      [
        // Missing second argument for match
        ("""$[?match(@.b)]""", "missing arg")
        // Unknown function name
        ("$[?unknown(@.b)]", "unknown func")
        // ValueType function as standalone test-expr (no comparison)
        ("$[?length(@.a)]", "length as test")
        // Negating a ValueType function
        ("$[?!length(@.a)]", "negate length")
        // LogicalType function in comparison
        ("""$[?match(@.a, "x") == true]""", "match in comparison")
      ]
    for (path_str, label) in invalid.values() do
      match \exhaustive\ JSONPathParser.parse(path_str)
      | let _: JSONPathParseError => None
      | let _: JSONPath =>
        h.fail("Expected error for (" + label + "): " + path_str)
      end
    end

// ===================================================================
// Example Tests — JSONPath Function Extension match/search
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterFunctionMatchSearch is UnitTest
  fun name(): String => "json/jsonpath/filter/function/match-search"

  fun apply(h: TestHelper) ? =>
    // RFC 9535 Section 2.4.6/2.4.7 example
    let doc = JSONObject
      .update(
        "a",
        JSONArray
          .push(I64(3))
          .push(I64(5))
          .push(I64(1))
          .push(I64(2))
          .push(I64(4))
          .push(I64(6))
          .push(JSONObject.update("b", "j"))
          .push(JSONObject.update("b", "k"))
          .push(JSONObject.update("b", JSONObject))
          .push(JSONObject.update("b", "kilo")))

    // match: full-string only — "j" and "k" match [jk], "kilo" does not
    let p1 = JSONPathParser.compile("""$.a[?match(@.b, "[jk]")]""")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // search: substring — "j", "k", and "kilo" all contain [jk]
    let p2 = JSONPathParser.compile("""$.a[?search(@.b, "[jk]")]""")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](3, r2.size())

    // Negated match
    let doc2 = JSONArray
      .push(JSONObject.update("name", "alice"))
      .push(JSONObject.update("name", "bob"))
      .push(JSONObject.update("name", "carol"))
    let p3 = JSONPathParser.compile("""$[?!match(@.name, "b.*")]""")?
    let r3 = p3.query(doc2)
    h.assert_eq[USize](2, r3.size())

    // Non-string args → false (numbers don't match)
    let doc3 = JSONArray
      .push(JSONObject.update("v", I64(42)))
      .push(JSONObject.update("v", "hello"))
    let p4 = JSONPathParser.compile("""$[?match(@.v, ".*")]""")?
    let r4 = p4.query(doc3)
    // Only "hello" matches — I64(42) is not a string
    h.assert_eq[USize](1, r4.size())

    // Invalid regex pattern → false (not crash)
    let doc4 = JSONArray
      .push(JSONObject.update("v", "test"))
    let p5 = JSONPathParser.compile("""$[?match(@.v, "[invalid")]""")?
    let r5 = p5.query(doc4)
    h.assert_eq[USize](0, r5.size())

    // match vs search: "abc" matches "abc" fully, "xabcx" does not
    let doc5 = JSONArray
      .push(JSONObject.update("v", "abc"))
      .push(JSONObject.update("v", "xabcx"))
    let p6 = JSONPathParser.compile("""$[?match(@.v, "abc")]""")?
    let r6 = p6.query(doc5)
    h.assert_eq[USize](1, r6.size()) // only exact "abc"

    let p7 = JSONPathParser.compile("""$[?search(@.v, "abc")]""")?
    let r7 = p7.query(doc5)
    h.assert_eq[USize](2, r7.size()) // both contain "abc"

// ===================================================================
// Example Tests — JSONPath Function Extension length
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterFunctionLength is UnitTest
  fun name(): String => "json/jsonpath/filter/function/length"

  fun apply(h: TestHelper) ? =>
    // String length — ASCII
    let doc1 = JSONArray
      .push(JSONObject.update("name", "Al"))
      .push(JSONObject.update("name", "Bob"))
      .push(JSONObject.update("name", "Carol"))
    let p1 = JSONPathParser.compile("$[?length(@.name) <= 3]")?
    let r1 = p1.query(doc1)
    h.assert_eq[USize](2, r1.size()) // "Al" (2) and "Bob" (3)

    // String length — Unicode multi-byte: "café" has 4 codepoints not 5 bytes
    let cafe: String val =
      recover val
        String
          .> append("caf")
          .> push(0xC3)
          .> push(0xA9)
      end
    let doc2 = JSONArray
      .push(JSONObject.update("s", cafe))
      .push(JSONObject.update("s", "hello"))
    let p2 = JSONPathParser.compile("$[?length(@.s) == 4]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size()) // only "café"

    // Array size
    let doc3 = JSONArray
      .push(JSONObject.update("items", JSONArray.push(I64(1)).push(I64(2))))
      .push(JSONObject.update("items", JSONArray.push(I64(1))))
    let p3 = JSONPathParser.compile("$[?length(@.items) > 1]")?
    let r3 = p3.query(doc3)
    h.assert_eq[USize](1, r3.size())

    // Object member count
    let doc4 = JSONArray
      .push(JSONObject.update(
        "obj",
        JSONObject.update("a", I64(1)).update("b", I64(2)).update("c", I64(3))))
      .push(JSONObject.update(
        "obj",
        JSONObject.update("x", I64(1))))
    let p4 = JSONPathParser.compile("$[?length(@.obj) >= 3]")?
    let r4 = p4.query(doc4)
    h.assert_eq[USize](1, r4.size())

    // length on number/bool/null/Nothing → Nothing, comparison
    // fails → 0 matches
    let doc5 = JSONArray
      .push(JSONObject.update("v", I64(42)))
      .push(JSONObject.update("v", true))
      .push(JSONObject.update("v", None))
      .push(JSONObject) // missing "v" → Nothing
    let p5 = JSONPathParser.compile("$[?length(@.v) > 0]")?
    let r5 = p5.query(doc5)
    h.assert_eq[USize](0, r5.size())

    // Nested: length(value(@.items))
    let doc6 = JSONObject
      .update(
        "items",
        JSONArray
          .push(JSONObject.update("x", "abc"))
          .push(JSONObject.update("x", "abcde")))
    let p6 = JSONPathParser.compile("$.items[?length(value(@.x)) > 3]")?
    let r6 = p6.query(doc6)
    h.assert_eq[USize](1, r6.size()) // only "abcde" (length 5)

// ===================================================================
// Example Tests — JSONPath Function Extension count
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterFunctionCount is UnitTest
  fun name(): String => "json/jsonpath/filter/function/count"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject
        .update("items", JSONArray.push(I64(1)).push(I64(2)).push(I64(3))))
      .push(JSONObject
        .update("items", JSONArray.push(I64(1))))
      .push(JSONObject
        .update("items", JSONArray))

    // count(@.items[*]) counts array elements
    let p1 = JSONPathParser.compile("$[?count(@.items[*]) > 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size()) // only first (3 items)

    // count(@.items[*]) == 0 for empty array
    let p2 = JSONPathParser.compile("$[?count(@.items[*]) == 0]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size()) // third element

    // count on object members via wildcard
    let doc2 = JSONArray
      .push(JSONObject.update(
        "data",
        JSONObject.update("a", I64(1)).update("b", I64(2))))
      .push(JSONObject.update(
        "data",
        JSONObject.update("x", I64(1))))
    let p3 = JSONPathParser.compile("$[?count(@.data.*) > 1]")?
    let r3 = p3.query(doc2)
    h.assert_eq[USize](1, r3.size())

    // count on non-container via wildcard → 0
    let doc3 = JSONArray
      .push(JSONObject.update("v", I64(42)))
    let p4 = JSONPathParser.compile("$[?count(@.v.*) > 0]")?
    let r4 = p4.query(doc3)
    h.assert_eq[USize](0, r4.size())

// ===================================================================
// Example Tests — JSONPath Function Extension value
// ===================================================================
class \nodoc\ iso _TestJSONPathFilterFunctionValue is UnitTest
  fun name(): String => "json/jsonpath/filter/function/value"

  fun apply(h: TestHelper) ? =>
    let doc = JSONArray
      .push(JSONObject
        .update("items", JSONArray.push(I64(10)).push(I64(20))))
      .push(JSONObject
        .update("items", JSONArray.push(I64(5))))
      .push(JSONObject
        .update("items", JSONArray))

    // value(@.items[0]) extracts single element
    let p1 = JSONPathParser.compile("$[?value(@.items[0]) > 7]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size()) // only first (10 > 7)

    // value on empty query → Nothing → comparison fails
    let p2 = JSONPathParser.compile("$[?value(@.missing) == 1]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](0, r2.size())

    // value on multi-element wildcard → Nothing
    let p3 = JSONPathParser.compile("$[?value(@.items[*]) == 5]")?
    let r3 = p3.query(doc)
    // First has 2 items (Nothing), second has 1 item (5), third has 0 (Nothing)
    h.assert_eq[USize](1, r3.size()) // only second

    // value must return Nothing for multi-element result, even if first matches
    let doc_multi = JSONArray
      .push(JSONObject
        .update("items", JSONArray.push(I64(5)).push(I64(99))))
    let p_multi = JSONPathParser.compile("$[?value(@.items[*]) == 5]")?
    let r_multi = p_multi.query(doc_multi)
    // 2 items → value returns Nothing, not 5
    h.assert_eq[USize](0, r_multi.size())

    // value used with string comparison
    let doc2 = JSONArray
      .push(JSONObject.update("tags", JSONArray.push("urgent")))
      .push(JSONObject.update("tags", JSONArray.push("low")))
    let p4 =
      JSONPathParser.compile(
        """$[?value(@.tags[0]) == "urgent"]""")?
    let r4 = p4.query(doc2)
    h.assert_eq[USize](1, r4.size())

// ===================================================================
// Regression Tests — stack-safe JSON walks (issue #5557)
// ===================================================================
//
// Every value-depth walk in the package (parse, print, JSONPath recursive
// descent, and filter `==` equality) is driven by an explicit work stack
// rather than native recursion, so nesting depth is bounded by the heap, not
// the scheduler thread's native stack. These tests drive each walk far past
// any practical input depth and assert it returns the correct result, so a
// logic error in the work-stack management surfaces here. They also guard
// against reintroducing native recursion, which on a build/platform that does
// not optimize the recursion's stack growth away crashes the process with an
// uncatchable stack overflow on deeply nested input — the bug in #5557. (Where
// the toolchain does elide that stack growth, recursion survives this depth, so
// these are not a universal crash-counterfactual.) See
// https://github.com/ponylang/ponyc/issues/5557.
primitive \nodoc\ _DeepNestingDepth
  """
  Nesting depth for the stack-safety regression tests: deep enough to overflow
  a native-recursion walk on a standard scheduler-thread stack (~8 MB) on
  builds and platforms that don't optimize the recursion's stack growth away,
  yet small enough to stay fast. The iterative walks handle it with depth
  bounded by the heap.
  """
  fun apply(): USize => 200_000

class \nodoc\ iso _TestParseDeeplyNested is UnitTest
  """
  Parsing deeply nested JSON completes (no native-stack overflow) and returns
  the right structure. Reaching past `parse` proves the walk finished; the
  root-array shape is checked too.
  """
  fun name(): String => "json/parse/deeply-nested"

  fun apply(h: TestHelper) =>
    let depth = _DeepNestingDepth()
    var s = recover iso String(depth * 2) end
    var i: USize = 0
    while i < depth do s.push('['); i = i + 1 end
    i = 0
    while i < depth do s.push(']'); i = i + 1 end
    match \exhaustive\ JSONParser.parse(consume s)
    | let a: JSONArray => h.assert_eq[USize](1, a.size())
    | let _: JSONValue => h.fail("expected a JSONArray at the root")
    | let e: JSONParseError => h.fail("deep parse failed: " + e.string())
    end

class \nodoc\ iso _TestPrintDeeplyNested is UnitTest
  """
  Serializing a deeply nested value completes (no native-stack overflow) and
  produces the right bytes. The value alternates object and array wrappers so
  both the object and array printer frames are driven at depth; the expected
  compact length is accumulated as the value is built.
  """
  fun name(): String => "json/print/deeply-nested"

  fun apply(h: TestHelper) =>
    let depth = _DeepNestingDepth()
    var v: JSONValue = JSONObject
    var expected_len: USize = 2 // innermost "{}"
    var i: USize = 0
    while i < depth do
      if (i % 2) == 0 then
        v = JSONArray.push(v)
        expected_len = expected_len + 2 // "[" ... "]"
      else
        v = JSONObject.update("a", v)
        expected_len = expected_len + 6 // {"a": ... }
      end
      i = i + 1
    end
    let s: String val = JSONPrinter.print(v)
    h.assert_eq[USize](expected_len, s.size())

class \nodoc\ iso _TestJSONPathDescendDeeplyNested is UnitTest
  """
  JSONPath recursive descent (`..`) over a deeply nested value completes (no
  native-stack overflow) and visits every node. `depth` nested objects each
  carry one "a", so `$..a` matches exactly `depth` values.
  """
  fun name(): String => "json/jsonpath/descend/deeply-nested"

  fun apply(h: TestHelper) ? =>
    let depth = _DeepNestingDepth()
    var v: JSONValue = JSONObject
    var i: USize = 0
    while i < depth do
      v = JSONObject.update("a", v)
      i = i + 1
    end
    let p = JSONPathParser.compile("$..a")?
    let r = p.query(v)
    h.assert_eq[USize](depth, r.size())

class \nodoc\ iso _TestJSONPathDescendOrder is UnitTest
  """
  Recursive descent must keep its pre-order, left-to-right traversal after the
  switch from native recursion to an explicit work stack (children are pushed
  in reverse so they pop in forward order). Built from arrays so child order is
  positional and predictable.
  """
  fun name(): String => "json/jsonpath/descend/order"

  fun apply(h: TestHelper) ? =>
    // [ {"a": 1}, {"a": [ {"a": 2}, {"a": 3} ]} ]
    let doc = JSONArray
      .push(JSONObject.update("a", I64(1)))
      .push(JSONObject.update(
        "a",
        JSONArray
          .push(JSONObject.update("a", I64(2)))
          .push(JSONObject.update("a", I64(3)))))
    let r = JSONPathParser.compile("$..a")?.query(doc)
    // pre-order, left-to-right: 1, then the inner array, then 2, then 3.
    h.assert_eq[USize](4, r.size())
    h.assert_eq[I64](1, r(0)? as I64)
    h.assert_eq[USize](2, (r(1)? as JSONArray).size())
    h.assert_eq[I64](2, r(2)? as I64)
    h.assert_eq[I64](3, r(3)? as I64)

class \nodoc\ iso _TestJSONPathFilterDeeplyNested is UnitTest
  """
  Filter equality (`==`) over deeply nested values completes (no native-stack
  overflow) and compares correctly. The document's single element is the deep
  value; `@ == @` forces a full structural comparison of it against itself
  (equality has no early-out when equal), which matches, giving one result.
  """
  fun name(): String => "json/jsonpath/filter/deeply-nested"

  fun apply(h: TestHelper) ? =>
    let depth = _DeepNestingDepth()
    var deep: JSONValue = JSONArray
    var i: USize = 0
    while i < depth do
      deep = JSONArray.push(deep)
      i = i + 1
    end
    let doc = JSONArray.push(deep)
    let p = JSONPathParser.compile("$[?@ == @]")?
    let r = p.query(doc)
    h.assert_eq[USize](1, r.size())

class \nodoc\ iso _TestTokenParserPositions is UnitTest
  """
  The streaming token parser reports correct byte offsets for every token in a
  nested document. In particular, an empty container's end token anchors
  token_start at its closing bracket — the same as a non-empty container —
  rather than at the opening bracket. Guards token_start/token_end bookkeeping.
  """
  fun name(): String => "json/tokenparser/positions"

  fun apply(h: TestHelper) =>
    let events = Array[(String, USize, USize)]
    let parser = JSONTokenParser(_TokenRecorder(events))
    try parser.feed("""{"a":[1,{}],"b":2,"c":[]}""")?
    else h.fail("token parse raised unexpectedly")
    end

    // (label, token_start, token_end) for {"a":[1,{}],"b":2,"c":[]}. The empty
    // {}'s ObjectEnd anchors token_start at 9 (its `}`) and the empty []'s
    // ArrayEnd at 23 (its `]`), the closing-bracket positions.
    let expected: Array[(String, USize, USize)] =
      [
        ("ObjectStart", 0, 1)
        ("Key", 1, 4)
        ("ArrayStart", 5, 6)
        ("Number", 6, 7)
        ("ObjectStart", 8, 9)
        ("ObjectEnd", 9, 10)
        ("ArrayEnd", 10, 11)
        ("Key", 12, 15)
        ("Number", 16, 17)
        ("Key", 18, 21)
        ("ArrayStart", 22, 23)
        ("ArrayEnd", 23, 24)
        ("ObjectEnd", 24, 25)
      ]
    h.assert_eq[USize](expected.size(), events.size())
    for (idx, exp) in expected.pairs() do
      try
        let got = events(idx)?
        h.assert_eq[String](exp._1, got._1)
        h.assert_eq[USize](exp._2, got._2)
        h.assert_eq[USize](exp._3, got._3)
      else
        h.fail("missing token event at index " + idx.string())
      end
    end

class \nodoc\ iso _TestTokenParserEndPosition is UnitTest
  """
  An end token's token_start marks where the closing bracket begins, whether
  or not the container is empty. Before ponylang/ponyc#5607 an empty container
  reported its opening bracket instead, so the span covered the whole `{}`/`[]`
  rather than just the closing bracket.
  """
  fun name(): String => "json/tokenparser/endposition"

  fun apply(h: TestHelper) =>
    // (input, end-token label, token_start, token_end). The end token is the
    // last token emitted for each of these single-value documents.
    _check(h, "{}", "ObjectEnd", 1, 2)
    _check(h, "[]", "ArrayEnd", 1, 2)
    _check(h, """{"a":1}""", "ObjectEnd", 6, 7)
    _check(h, "[1]", "ArrayEnd", 2, 3)
    // Interior whitespace separates the closing bracket from "just past the
    // opening bracket" — without it both land at the same offset, so these
    // pin the anchor to the closing bracket specifically.
    _check(h, "{ }", "ObjectEnd", 2, 3)
    _check(h, "[ ]", "ArrayEnd", 2, 3)

  fun _check(
    h: TestHelper,
    input: String,
    label: String,
    expected_start: USize,
    expected_end: USize)
  =>
    let events = Array[(String, USize, USize)]
    let parser = JSONTokenParser(_TokenRecorder(events))
    try parser.feed(input)?
    else h.fail("token parse raised unexpectedly for " + input); return
    end
    try
      let last = events(events.size() - 1)?
      h.assert_eq[String](label, last._1)
      h.assert_eq[USize](expected_start, last._2)
      h.assert_eq[USize](expected_end, last._3)
    else
      h.fail("no tokens recorded for " + input)
    end

class \nodoc\ iso _TestTokenParserStringPosition is UnitTest
  """
  A String or Key token's token_start marks the opening quote, so its span
  covers the whole quoted token — the same anchoring every other token uses.
  Previously token_start pointed one byte past the opening quote, dropping the
  opening quote from the span.
  """
  fun name(): String => "json/tokenparser/stringposition"

  fun apply(h: TestHelper) =>
    // (input, label, token_start, token_end) for the first String/Key token.
    // The span covers the opening quote, the content, and the closing quote.
    _check(h, "\"abc\"", "String", 0, 5)
    _check(h, "\"\"", "String", 0, 2)
    _check(h, """{"k":1}""", "Key", 1, 4)
    _check(h, "[\"x\"]", "String", 1, 4)

  fun _check(
    h: TestHelper,
    input: String,
    label: String,
    expected_start: USize,
    expected_end: USize)
  =>
    let events = Array[(String, USize, USize)]
    let parser = JSONTokenParser(_TokenRecorder(events))
    try parser.feed(input)?
    else h.fail("token parse raised unexpectedly for " + input); return
    end
    for ev in events.values() do
      if (ev._1 == "String") or (ev._1 == "Key") then
        h.assert_eq[String](label, ev._1)
        h.assert_eq[USize](expected_start, ev._2)
        h.assert_eq[USize](expected_end, ev._3)
        return
      end
    end
    h.fail("no String/Key token recorded for " + input)

class \nodoc\ _TokenRecorder is JSONTokenNotify
  """Records (label, token_start, token_end) into a caller-owned array."""
  let _events: Array[(String, USize, USize)]

  new create(events: Array[(String, USize, USize)]) =>
    _events = events

  fun ref apply(parser: JSONTokenParser, token: JSONToken) =>
    let label =
      match \exhaustive\ token
      | JSONTokenNull => "Null"
      | JSONTokenTrue => "True"
      | JSONTokenFalse => "False"
      | let _: JSONTokenNumber => "Number"
      | let _: JSONTokenString => "String"
      | let _: JSONTokenKey => "Key"
      | JSONTokenObjectStart => "ObjectStart"
      | JSONTokenObjectEnd => "ObjectEnd"
      | JSONTokenArrayStart => "ArrayStart"
      | JSONTokenArrayEnd => "ArrayEnd"
      end
    _events.push((label, parser.token_start(), parser.token_end()))

