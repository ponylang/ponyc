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
    test(Property1UnitTest[String](_JsonPathSafetyProperty))
    test(Property1UnitTest[String](_ObjectRemoveProperty))
    test(Property1UnitTest[(String, String)](_ObjectSizeProperty))
    test(Property1UnitTest[(String, I64)](_ObjectUpdateApplyProperty))
    test(Property1UnitTest[String](_ParsePrintRoundtripProperty))
    test(Property1UnitTest[String](_StringEscapeRoundtripProperty))
    // Example tests
    test(_TestArrayUpdate)
    test(_TestJsonPathFilterAbsoluteQuery)
    test(_TestJsonPathFilterComparison)
    test(_TestJsonPathFilterDeepEquality)
    test(_TestJsonPathFilterExistence)
    test(_TestJsonPathFilterLogical)
    test(_TestJsonPathFilterFunctionCount)
    test(_TestJsonPathFilterFunctionLength)
    test(_TestJsonPathFilterFunctionMatchSearch)
    test(_TestJsonPathFilterFunctionParse)
    test(_TestJsonPathFilterFunctionValue)
    test(_TestJsonPathFilterNested)
    test(_TestJsonPathFilterNothing)
    test(_TestJsonPathFilterNumbers)
    test(_TestJsonPathFilterOnObjects)
    test(_TestJsonPathFilterParse)
    test(_TestJsonPathFilterTypes)
    test(_TestJsonPathParse)
    test(_TestJsonPathParseErrors)
    test(_TestJsonPathQueryAdvanced)
    test(_TestJsonPathQueryBasic)
    test(_TestJsonPathQueryComplex)
    test(_TestJsonPathQuerySliceStep)
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
    test(_TestParseNumbers)
    test(_TestParseStrings)
    test(_TestParseWholeDocument)
    test(_TestPrintCompact)
    test(_TestPrintFloats)
    test(_TestPrintPretty)
    test(_TestTokenParserAbort)

// ===================================================================
// Generators
// ===================================================================

primitive \nodoc\ _JsonValueStringGen
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
    let choice = if depth == 0 then
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
    let denom: I64 = match rnd.usize(0, 3)
    | 0 => 2
    | 1 => 4
    | 2 => 5
    else 10
    end
    let f = numerator.f64() / denom.f64()
    let s: String val = f.string()
    if
      (not s.contains("."))
        and (not s.contains("e"))
        and (not s.contains("E"))
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
      let c = rnd.u8(0x20, 0x7E)
      if c == '"' then
        buf.append("\\\"")
      elseif c == '\\' then
        buf.append("\\\\")
      else
        buf.push(c)
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
    _JsonValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // compact(parse(s)) is a fixpoint after one cycle
    let first_parse = JsonParser.parse(sample)
    match \exhaustive\ first_parse
    | let j1: JsonValue =>
      let s1: String val = _JsonPrint.compact(j1)
      match \exhaustive\ JsonParser.parse(s1)
      | let j2: JsonValue =>
        let s2: String val = _JsonPrint.compact(j2)
        ph.assert_eq[String val](s1, s2)
      | let e: JsonParseError =>
        ph.fail("Re-parse failed: " + e.string())
      end
    | let e: JsonParseError =>
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
    match \exhaustive\ JsonParser.parse(s)
    | let j: JsonValue =>
      try
        let parsed = j as I64
        ph.assert_eq[I64](sample, parsed)
      else
        ph.fail("Parsed as wrong type for: " + s)
      end
    | let e: JsonParseError =>
      ph.fail("Parse failed for: " + s + " — " + e.string())
    end

class \nodoc\ iso _F64RoundtripProperty is Property1[F64]
  fun name(): String => "json/roundtrip/f64"

  fun gen(): Generator[F64] =>
    // Generate clean-roundtripping F64 values
    Generator[F64](
      object is GenObj[F64]
        fun generate(rnd: Randomness): F64 =>
          let n = rnd.i64(-100, 100)
          let d: I64 = match rnd.usize(0, 3)
          | 0 => 2
          | 1 => 4
          | 2 => 5
          else 10
          end
          n.f64() / d.f64()
      end)

  fun ref property(sample: F64, ph: PropertyHelper) =>
    // Serialize as a JSON array element to handle the formatting
    let arr = JsonArray.push(sample)
    let s: String val = _JsonPrint.compact(arr)
    match \exhaustive\ JsonParser.parse(s)
    | let j: JsonValue =>
      try
        let parsed_arr = j as JsonArray
        let parsed = parsed_arr(0)? as F64
        ph.assert_eq[F64](sample, parsed)
      else
        ph.fail("Type mismatch after roundtrip for: " + sample.string())
      end
    | let e: JsonParseError =>
      ph.fail("Parse failed for: " + s + " — " + e.string())
    end

class \nodoc\ iso _StringEscapeRoundtripProperty is Property1[String]
  fun name(): String => "json/roundtrip/string-escape"

  fun gen(): Generator[String] =>
    Generators.ascii(0, 50)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // Embed string in a JSON array, serialize, parse, extract
    let arr = JsonArray.push(sample)
    let serialized: String val = _JsonPrint.compact(arr)
    match \exhaustive\ JsonParser.parse(serialized)
    | let j: JsonValue =>
      try
        let parsed_arr = j as JsonArray
        let recovered = parsed_arr(0)? as String
        ph.assert_eq[String val](sample, recovered)
      else
        ph.fail("Type mismatch in string roundtrip")
      end
    | let e: JsonParseError =>
      ph.fail("Parse failed: " + e.string())
    end

// ===================================================================
// Property Tests — JsonObject
// ===================================================================

class \nodoc\ iso _ObjectUpdateApplyProperty is Property1[(String, I64)]
  fun name(): String => "json/object/update-apply"

  fun gen(): Generator[(String, I64)] =>
    Generators.zip2[String, I64](
      Generators.ascii_letters(1, 10),
      Generators.i64(-1000, 1000))

  fun ref property(sample: (String, I64), ph: PropertyHelper) ? =>
    (let key, let value) = sample
    let obj = JsonObject.update(key, value)
    let got = obj(key)? as I64
    ph.assert_eq[I64](value, got)

class \nodoc\ iso _ObjectRemoveProperty is Property1[String]
  fun name(): String => "json/object/remove"

  fun gen(): Generator[String] =>
    Generators.ascii_letters(1, 10)

  fun ref property(sample: String, ph: PropertyHelper) =>
    let obj = JsonObject.update(sample, I64(42))
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
    let obj1 = JsonObject.update(k1, I64(1))
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
// Property Tests — JsonArray
// ===================================================================

class \nodoc\ iso _ArrayPushApplyProperty is Property1[I64]
  fun name(): String => "json/array/push-apply"

  fun gen(): Generator[I64] =>
    Generators.i64()

  fun ref property(sample: I64, ph: PropertyHelper) ? =>
    let arr = JsonArray.push(sample)
    let got = arr(arr.size() - 1)? as I64
    ph.assert_eq[I64](sample, got)

class \nodoc\ iso _ArrayPushPopProperty is Property1[I64]
  fun name(): String => "json/array/push-pop"

  fun gen(): Generator[I64] =>
    Generators.i64()

  fun ref property(sample: I64, ph: PropertyHelper) ? =>
    let base = JsonArray.push(I64(99))
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
    var arr = JsonArray
    var i: USize = 0
    while i < sample do
      arr = arr.push(I64(i.i64()))
      i = i + 1
    end
    ph.assert_eq[USize](sample, arr.size())

// ===================================================================
// Property Tests — JSONPath Safety
// ===================================================================

class \nodoc\ iso _JsonPathSafetyProperty is Property1[String]
  fun name(): String => "json/jsonpath/safety"

  fun gen(): Generator[String] =>
    _JsonValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    // Parse the generated JSON
    match \exhaustive\ JsonParser.parse(sample)
    | let doc: JsonValue =>
      // A set of valid paths — none should crash
      let paths: Array[String] val = [
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
          let path = JsonPathParser.compile(path_str)?
          // query should never crash — it returns an array (possibly empty)
          let results = path.query(doc)
          // Just verify we got an array back (size >= 0)
          ph.assert_true(results.size() >= 0)
        else
          ph.fail("Failed to compile known-valid path: " + path_str)
        end
      end
    | let _: JsonParseError =>
      // Generator produced invalid JSON — shouldn't happen but skip
      None
    end

// ===================================================================
// Example Tests — Parser Success
// ===================================================================

class \nodoc\ iso _TestParseKeywords is UnitTest
  fun name(): String => "json/parse/keywords"

  fun apply(h: TestHelper) ? =>
    match JsonParser.parse("true")
    | let j: JsonValue => h.assert_eq[Bool](true, j as Bool)
    else h.fail("true failed to parse")
    end

    match JsonParser.parse("false")
    | let j: JsonValue => h.assert_eq[Bool](false, j as Bool)
    else h.fail("false failed to parse")
    end

    match JsonParser.parse("null")
    | let j: JsonValue =>
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
    match JsonParser.parse("0")
    | let j: JsonValue => h.assert_eq[I64](0, j as I64)
    else h.fail("0 failed")
    end

    match JsonParser.parse("42")
    | let j: JsonValue => h.assert_eq[I64](42, j as I64)
    else h.fail("42 failed")
    end

    match JsonParser.parse("-1")
    | let j: JsonValue => h.assert_eq[I64](-1, j as I64)
    else h.fail("-1 failed")
    end

    // Floats
    match JsonParser.parse("3.14")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true((f - 3.14).abs() < 1e-10)
    else h.fail("3.14 failed")
    end

    match JsonParser.parse("1e10")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true((f - 1e10).abs() < 1.0)
    else h.fail("1e10 failed")
    end

    match JsonParser.parse("1.5e-3")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true((f - 0.0015).abs() < 1e-10)
    else h.fail("1.5e-3 failed")
    end

    match JsonParser.parse("-0.5")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true((f - (-0.5)).abs() < 1e-10)
    else h.fail("-0.5 failed")
    end

    // Large integer promoted to F64 instead of overflowing
    match JsonParser.parse("99999999999999999999")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true(f > 9.99e18)
    else h.fail("large integer failed")
    end

    // Zero alone is valid
    match JsonParser.parse("0")
    | let j: JsonValue => h.assert_eq[I64](0, j as I64)
    else h.fail("standalone 0 failed")
    end

    // 0.5 is valid (zero before decimal)
    match JsonParser.parse("0.5")
    | let j: JsonValue =>
      let f = j as F64
      h.assert_true((f - 0.5).abs() < 1e-10)
    else h.fail("0.5 failed")
    end

class \nodoc\ iso _TestParseStrings is UnitTest
  fun name(): String => "json/parse/strings"

  fun apply(h: TestHelper) ? =>
    // Simple string
    match JsonParser.parse("\"hello\"")
    | let j: JsonValue => h.assert_eq[String]("hello", j as String)
    else h.fail("simple string failed")
    end

    // All basic escape sequences
    match JsonParser.parse("\"a\\nb\\tc\\\"d\\\\e\\/f\"")
    | let j: JsonValue =>
      let s = j as String
      h.assert_eq[String]("a\nb\tc\"d\\e/f", s)
    else h.fail("escape sequences failed")
    end

    // \b and \f
    match JsonParser.parse("\"\\b\\f\"")
    | let j: JsonValue =>
      let s = j as String
      h.assert_eq[U8](0x08, try s(0)? else 0 end)
      h.assert_eq[U8](0x0C, try s(1)? else 0 end)
    else h.fail("\\b\\f failed")
    end

    // \r
    match JsonParser.parse("\"\\r\"")
    | let j: JsonValue =>
      h.assert_eq[String]("\r", j as String)
    else h.fail("\\r failed")
    end

    // Unicode BMP: \u00E9 = é
    match JsonParser.parse("\"\\u00E9\"")
    | let j: JsonValue =>
      let s = j as String
      let expected = recover val String.from_utf32(0xE9) end
      h.assert_eq[String](expected, s)
    else h.fail("unicode BMP failed")
    end

    // Surrogate pair: \uD834\uDD1E = U+1D11E (musical symbol G clef)
    match JsonParser.parse("\"\\uD834\\uDD1E\"")
    | let j: JsonValue =>
      let s = j as String
      let expected = recover val String.from_utf32(0x1D11E) end
      h.assert_eq[String](expected, s)
    else h.fail("surrogate pair failed")
    end

    // Control char via unicode escape: \u001F
    match JsonParser.parse("\"\\u001F\"")
    | let j: JsonValue =>
      let s = j as String
      h.assert_eq[USize](1, s.size())
      h.assert_eq[U8](0x1F, try s(0)? else 0 end)
    else h.fail("control char escape failed")
    end

class \nodoc\ iso _TestParseContainers is UnitTest
  fun name(): String => "json/parse/containers"

  fun apply(h: TestHelper) ? =>
    // Empty object
    match \exhaustive\ JsonParser.parse("{}")
    | let j: JsonValue =>
      let obj = j as JsonObject
      h.assert_eq[USize](0, obj.size())
    else h.fail("empty object failed")
    end

    // Empty array
    match JsonParser.parse("[]")
    | let j: JsonValue =>
      let arr = j as JsonArray
      h.assert_eq[USize](0, arr.size())
    else h.fail("empty array failed")
    end

    // Nested structure
    match JsonParser.parse("""{"a":{"b":[1,2]}}""")
    | let j: JsonValue =>
      let nav = JsonNav(j)
      h.assert_eq[I64](1, nav("a")("b")(USize(0)).as_i64()?)
      h.assert_eq[I64](2, nav("a")("b")(USize(1)).as_i64()?)
    else h.fail("nested structure failed")
    end

    // Whitespace between tokens
    match JsonParser.parse("  { \"a\" :  1  ,  \"b\" :  2  }  ")
    | let j: JsonValue =>
      let obj = j as JsonObject
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
    match \exhaustive\ JsonParser.parse(src)
    | let j: JsonValue =>
      let nav = JsonNav(j)
      h.assert_eq[String]("A", nav("store")("book")(USize(0))("title").as_string()?)
      h.assert_eq[String]("Y", nav("store")("book")(USize(1))("author").as_string()?)
      h.assert_eq[I64](15, nav("store")("bicycle")("price").as_i64()?)
      h.assert_eq[String]("red", nav("store")("bicycle")("color").as_string()?)
    | let e: JsonParseError =>
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
    _assert_parse_error(h, "\"hello", "unterminated string")
    _assert_parse_error(h, "\"\\x\"", "bad escape")
    _assert_parse_error(h, "1 2", "trailing content")
    _assert_parse_error(h, "\"\\u00GG\"", "bad unicode hex")

    // Leading zeros (RFC 8259)
    _assert_parse_error(h, "01", "leading zero")
    _assert_parse_error(h, "007", "leading zeros")
    _assert_parse_error(h, "00", "double zero")
    _assert_parse_error(h, "-01", "negative leading zero")

    // Raw control char (byte < 0x20)
    let ctrl = recover val
      let s = String(3)
      s.push('"')
      s.push(0x01)
      s.push('"')
      s
    end
    _assert_parse_error(h, ctrl, "raw control char")

  fun _assert_parse_error(h: TestHelper, input: String, label: String) =>
    match \exhaustive\ JsonParser.parse(input)
    | let _: JsonParseError => None // expected
    | let _: JsonValue => h.fail("Expected error for: " + label)
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
    match \exhaustive\ JsonParser.parse(input)
    | let _: JsonParseError => None // expected
    | let _: JsonValue => h.fail("Expected error for: " + label)
    end

// ===================================================================
// Example Tests — Serialization
// ===================================================================

class \nodoc\ iso _TestPrintCompact is UnitTest
  fun name(): String => "json/print/compact"

  fun apply(h: TestHelper) =>
    // Empty containers
    h.assert_eq[String]("{}", JsonObject.string())
    h.assert_eq[String]("[]", JsonArray.string())

    // Object with entries
    let obj = JsonObject.update("a", I64(1))
    let obj_s: String val = obj.string()
    h.assert_eq[String]("""{"a":1}""", obj_s)

    // Array with entries
    let arr = JsonArray.push(I64(1)).push(I64(2))
    let arr_s: String val = arr.string()
    h.assert_eq[String]("[1,2]", arr_s)

    // Boolean, null via array
    let mixed = JsonArray
      .push(true)
      .push(false)
      .push(None)
    let mixed_s: String val = mixed.string()
    h.assert_eq[String]("[true,false,null]", mixed_s)

    // String with special chars
    let str_arr = JsonArray.push("a\"b\\c\nd")
    let str_s: String val = str_arr.string()
    h.assert_eq[String]("""["a\"b\\c\nd"]""", str_s)

class \nodoc\ iso _TestPrintPretty is UnitTest
  fun name(): String => "json/print/pretty"

  fun apply(h: TestHelper) =>
    // Empty containers stay compact
    h.assert_eq[String]("{}", JsonObject.pretty_string())
    h.assert_eq[String]("[]", JsonArray.pretty_string())

    // Simple object
    let obj = JsonObject.update("a", I64(1))
    let expected = "{\n  \"a\": 1\n}"
    h.assert_eq[String](expected, obj.pretty_string())

    // Nested
    let inner = JsonObject.update("x", I64(42))
    let outer = JsonObject.update("inner", inner)
    let nested_s: String val = outer.pretty_string()
    h.assert_true(nested_s.contains("    \"x\": 42"))

    // Custom indent
    let tab_s: String val = obj.pretty_string("\t")
    h.assert_true(tab_s.contains("\t\"a\": 1"))

    // Array
    let arr = JsonArray.push(I64(1)).push(I64(2))
    let arr_s: String val = arr.pretty_string()
    let arr_expected = "[\n  1,\n  2\n]"
    h.assert_eq[String](arr_expected, arr_s)

class \nodoc\ iso _TestPrintFloats is UnitTest
  fun name(): String => "json/print/floats"

  fun apply(h: TestHelper) =>
    // Whole-number float gets .0 suffix
    let whole = JsonArray.push(F64(1))
    let whole_s: String val = whole.string()
    h.assert_eq[String]("[1.0]", whole_s)

    // Decimal float kept as-is
    let dec = JsonArray.push(F64(1.5))
    let dec_s: String val = dec.string()
    h.assert_eq[String]("[1.5]", dec_s)

    // Negative float
    let neg = JsonArray.push(F64(-3.25))
    let neg_s: String val = neg.string()
    h.assert_eq[String]("[-3.25]", neg_s)

    // Zero
    let zero = JsonArray.push(F64(0))
    let zero_s: String val = zero.string()
    h.assert_eq[String]("[0.0]", zero_s)

// ===================================================================
// Example Tests — Collections
// ===================================================================

class \nodoc\ iso _TestObjectGetOrElse is UnitTest
  fun name(): String => "json/object/get-or-else"

  fun apply(h: TestHelper) ? =>
    let obj = JsonObject.update("key", I64(42))

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
    let arr = JsonArray.push(I64(1)).push(I64(2)).push(I64(3))

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
    let doc = JsonObject
      .update("name", "Alice")
      .update("age", I64(30))
      .update("score", F64(9.5))
      .update("active", true)
      .update("data", None)
      .update("tags", JsonArray.push("a").push("b"))
      .update("meta", JsonObject.update("x", I64(1)))

    let nav = JsonNav(doc)

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
    let obj = JsonObject.update("a", I64(1))
    let arr = JsonArray.push(I64(1))
    let nav_obj = JsonNav(obj)
    let nav_arr = JsonNav(arr)

    // Missing key
    h.assert_false(nav_obj("missing").found())

    // Out of bounds index
    h.assert_false(nav_arr(USize(99)).found())

    // Type mismatch: string key on array
    h.assert_false(nav_arr("key").found())

    // Type mismatch: index on object
    h.assert_false(nav_obj(USize(0)).found())

    // JsonNotFound propagates through chain
    h.assert_false(nav_obj("x")("y")("z").found())

    // Extractor on JsonNotFound raises
    h.assert_error({() ? => nav_obj("missing").as_string()? })

class \nodoc\ iso _TestNavInspection is UnitTest
  fun name(): String => "json/nav/inspection"

  fun apply(h: TestHelper) ? =>
    let obj = JsonObject.update("a", I64(1)).update("b", I64(2))
    let arr = JsonArray.push(I64(1)).push(I64(2)).push(I64(3))

    // found()
    h.assert_true(JsonNav(obj).found())
    h.assert_false(JsonNav(obj)("missing").found())

    // size() on object and array
    h.assert_eq[USize](2, JsonNav(obj).size()?)
    h.assert_eq[USize](3, JsonNav(arr).size()?)

    // size() raises on non-container
    h.assert_error({() ? => JsonNav(I64(1)).size()? })

    // json() returns raw value
    match JsonNav(obj).json()
    | let o: JsonObject => h.assert_eq[USize](2, o.size())
    else h.fail("json() returned wrong type")
    end

    match JsonNav(obj)("missing").json()
    | JsonNotFound => None // expected
    else h.fail("Expected JsonNotFound from json()")
    end

// ===================================================================
// Example Tests — Lens
// ===================================================================

class \nodoc\ iso _TestLensGet is UnitTest
  fun name(): String => "json/lens/get"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", JsonObject.update("b", I64(42)))

    // Identity lens returns root
    match JsonLens.get(doc)
    | let j: JsonValue =>
      let obj = j as JsonObject
      h.assert_true(obj.contains("a"))
    else h.fail("Identity get failed")
    end

    // Nested path
    let lens = JsonLens("a")("b")
    match lens.get(doc)
    | let j: JsonValue => h.assert_eq[I64](42, j as I64)
    else h.fail("Nested get failed")
    end

    // Missing intermediate -> JsonNotFound
    let missing = JsonLens("x")("y")
    match \exhaustive\ missing.get(doc)
    | JsonNotFound => None
    else h.fail("Expected JsonNotFound for missing path")
    end

    // Type mismatch -> JsonNotFound
    let mismatch = JsonLens("a")("b")("c")
    match \exhaustive\ mismatch.get(doc)
    | JsonNotFound => None
    else h.fail("Expected JsonNotFound for type mismatch")
    end

class \nodoc\ iso _TestLensSet is UnitTest
  fun name(): String => "json/lens/set"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", JsonObject
        .update("b", I64(1))
        .update("c", I64(2)))

    // Identity lens replaces root
    match JsonLens.set(doc, I64(99))
    | let j: JsonValue => h.assert_eq[I64](99, j as I64)
    else h.fail("Identity set failed")
    end

    // Nested set
    let lens = JsonLens("a")("b")
    match lens.set(doc, I64(42))
    | let j: JsonValue =>
      let nav = JsonNav(j)
      h.assert_eq[I64](42, nav("a")("b").as_i64()?)
      // Sibling preserved
      h.assert_eq[I64](2, nav("a")("c").as_i64()?)
    else h.fail("Nested set failed")
    end

    // Original unchanged
    let nav = JsonNav(doc)
    h.assert_eq[I64](1, nav("a")("b").as_i64()?)

    // Missing intermediate -> JsonNotFound
    let missing = JsonLens("x")("y")
    match \exhaustive\ missing.set(doc, I64(1))
    | JsonNotFound => None
    else h.fail("Expected JsonNotFound for missing intermediate")
    end

class \nodoc\ iso _TestLensRemove is UnitTest
  fun name(): String => "json/lens/remove"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", JsonObject
        .update("b", I64(1))
        .update("c", I64(2)))

    // Remove key
    let lens = JsonLens("a")("b")
    match lens.remove(doc)
    | let j: JsonValue =>
      let nav = JsonNav(j)
      h.assert_false(nav("a")("b").found())
      // Sibling preserved
      h.assert_eq[I64](2, nav("a")("c").as_i64()?)
    else h.fail("Remove failed")
    end

    // Remove on array index -> JsonNotFound
    let arr_doc = JsonObject.update("arr", JsonArray.push(I64(1)))
    let arr_lens = JsonLens("arr")(USize(0))
    match \exhaustive\ arr_lens.remove(arr_doc)
    | JsonNotFound => None
    else h.fail("Expected JsonNotFound for array index remove")
    end

class \nodoc\ iso _TestLensComposition is UnitTest
  fun name(): String => "json/lens/composition"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", JsonObject
        .update("b", JsonObject
          .update("c", I64(99))))

    // compose equivalent to chained apply
    let lens_ab = JsonLens("a")("b")
    let lens_c = JsonLens("c")
    let composed = lens_ab.compose(lens_c)
    let chained = JsonLens("a")("b")("c")

    match composed.get(doc)
    | let j1: JsonValue =>
      match chained.get(doc)
      | let j2: JsonValue =>
        h.assert_eq[I64](j1 as I64, j2 as I64)
      else h.fail("Chained get failed")
      end
    else h.fail("Composed get failed")
    end

    // or_else falls back when first lens fails
    let missing = JsonLens("x")
    let found = JsonLens("a")("b")("c")
    let fallback = missing.or_else(found)
    match fallback.get(doc)
    | let j: JsonValue => h.assert_eq[I64](99, j as I64)
    else h.fail("or_else fallback failed")
    end

    // or_else uses first when it succeeds
    let first_wins = found.or_else(missing)
    match \exhaustive\ first_wins.get(doc)
    | let j: JsonValue => h.assert_eq[I64](99, j as I64)
    else h.fail("or_else first-match failed")
    end

    // Composed set modifies deeply nested value
    match composed.set(doc, I64(0))
    | let j: JsonValue =>
      let nav = JsonNav(j)
      h.assert_eq[I64](0, nav("a")("b")("c").as_i64()?)
    else h.fail("Composed set failed")
    end

// ===================================================================
// Example Tests — JSONPath
// ===================================================================

class \nodoc\ iso _TestJsonPathParse is UnitTest
  fun name(): String => "json/jsonpath/parse"

  fun apply(h: TestHelper) =>
    // All valid expressions should parse
    let valid: Array[String] val = [
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
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPath => None // pass
      | let e: JsonPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // compile raises on bad input
    h.assert_error({() ? => JsonPathParser.compile("invalid")? })

    // compile succeeds on good input
    try
      JsonPathParser.compile("$.a")?
    else
      h.fail("compile should succeed for $.a")
    end

class \nodoc\ iso _TestJsonPathParseErrors is UnitTest
  fun name(): String => "json/jsonpath/parse-errors"

  fun apply(h: TestHelper) =>
    let invalid: Array[String] val = [
      ""         // empty string
      "name"     // missing $
      "$!"       // bad segment char
      "$[0"      // unclosed bracket
      "$['open"  // unterminated string
    ]
    for path_str in invalid.values() do
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPathParseError => None // expected
      | let _: JsonPath =>
        h.fail("Expected error for: " + path_str)
      end
    end

class \nodoc\ iso _TestJsonPathQueryBasic is UnitTest
  fun name(): String => "json/jsonpath/query/basic"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", I64(1))
      .update("b", JsonObject.update("c", I64(2)))

    let arr_doc = JsonArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))

    // Dot child
    let p1 = JsonPathParser.compile("$.a")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())
    h.assert_eq[I64](1, r1(0)? as I64)

    // Nested dots
    let p2 = JsonPathParser.compile("$.b.c")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())
    h.assert_eq[I64](2, r2(0)? as I64)

    // Index
    let p3 = JsonPathParser.compile("$[0]")?
    let r3 = p3.query(arr_doc)
    h.assert_eq[USize](1, r3.size())
    h.assert_eq[I64](10, r3(0)? as I64)

    // Negative index
    let p4 = JsonPathParser.compile("$[-1]")?
    let r4 = p4.query(arr_doc)
    h.assert_eq[USize](1, r4.size())
    h.assert_eq[I64](30, r4(0)? as I64)

    // Missing key -> empty
    let p5 = JsonPathParser.compile("$.missing")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](0, r5.size())

    // Type mismatch -> empty
    let p6 = JsonPathParser.compile("$.a")?
    let r6 = p6.query(arr_doc)
    h.assert_eq[USize](0, r6.size())

    // query_one returns first
    match p1.query_one(doc)
    | let j: JsonValue => h.assert_eq[I64](1, j as I64)
    else h.fail("query_one should find $.a")
    end

    // query_one returns JsonNotFound when empty
    match \exhaustive\ p5.query_one(doc)
    | JsonNotFound => None
    else h.fail("query_one should return JsonNotFound for missing")
    end

class \nodoc\ iso _TestJsonPathQueryAdvanced is UnitTest
  fun name(): String => "json/jsonpath/query/advanced"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("a", I64(1))
      .update("b", I64(2))
      .update("c", JsonObject.update("a", I64(3)))

    let arr = JsonArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))
      .push(I64(40))

    // Wildcard on object
    let p1 = JsonPathParser.compile("$.*")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](3, r1.size())

    // Wildcard on array
    let p2 = JsonPathParser.compile("$[*]")?
    let r2 = p2.query(arr)
    h.assert_eq[USize](4, r2.size())

    // Recursive descent
    let p3 = JsonPathParser.compile("$..a")?
    let r3 = p3.query(doc)
    // Should find doc.a (1) and doc.c.a (3)
    h.assert_eq[USize](2, r3.size())

    // Slice [0:2]
    let p4 = JsonPathParser.compile("$[0:2]")?
    let r4 = p4.query(arr)
    h.assert_eq[USize](2, r4.size())
    h.assert_eq[I64](10, r4(0)? as I64)
    h.assert_eq[I64](20, r4(1)? as I64)

    // Open-ended slices
    let p5 = JsonPathParser.compile("$[:2]")?
    let r5 = p5.query(arr)
    h.assert_eq[USize](2, r5.size())

    let p6 = JsonPathParser.compile("$[1:]")?
    let r6 = p6.query(arr)
    h.assert_eq[USize](3, r6.size())
    h.assert_eq[I64](20, r6(0)? as I64)

    // Negative slice
    let p7 = JsonPathParser.compile("$[-2:]")?
    let r7 = p7.query(arr)
    h.assert_eq[USize](2, r7.size())
    h.assert_eq[I64](30, r7(0)? as I64)
    h.assert_eq[I64](40, r7(1)? as I64)

    // Union
    let p8 = JsonPathParser.compile("$[0,2]")?
    let r8 = p8.query(arr)
    h.assert_eq[USize](2, r8.size())
    h.assert_eq[I64](10, r8(0)? as I64)
    h.assert_eq[I64](30, r8(1)? as I64)

    // Descendant wildcard
    let p9 = JsonPathParser.compile("$..*")?
    let r9 = p9.query(doc)
    // Should include all values at all levels
    h.assert_true(r9.size() > 0)

class \nodoc\ iso _TestJsonPathQueryComplex is UnitTest
  fun name(): String => "json/jsonpath/query/complex"

  fun apply(h: TestHelper) ? =>
    let book1 = JsonObject
      .update("title", "A")
      .update("author", "X")
      .update("price", I64(10))

    let book2 = JsonObject
      .update("title", "B")
      .update("author", "Y")
      .update("price", I64(20))

    let bicycle = JsonObject
      .update("color", "red")
      .update("price", I64(15))

    let store = JsonObject
      .update("book", JsonArray.push(book1).push(book2))
      .update("bicycle", bicycle)

    let doc = JsonObject.update("store", store)

    // All book authors
    let p1 = JsonPathParser.compile("$.store.book[*].author")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // All prices (recursive descent)
    let p2 = JsonPathParser.compile("$.store..price")?
    let r2 = p2.query(doc)
    // 2 book prices + 1 bicycle price = 3
    h.assert_eq[USize](3, r2.size())

    // First book title
    let p3 = JsonPathParser.compile("$.store.book[0].title")?
    match p3.query_one(doc)
    | let j: JsonValue => h.assert_eq[String]("A", j as String)
    else h.fail("Should find first book title")
    end

class \nodoc\ iso _TestJsonPathQuerySliceStep is UnitTest
  fun name(): String => "json/jsonpath/query/slice-step"

  fun apply(h: TestHelper) ? =>
    let arr = JsonArray
      .push(I64(10))
      .push(I64(20))
      .push(I64(30))
      .push(I64(40))
      .push(I64(50))

    // Positive step: every other element [0:5:2] -> [10, 30, 50]
    let p1 = JsonPathParser.compile("$[0:5:2]")?
    let r1 = p1.query(arr)
    h.assert_eq[USize](3, r1.size())
    h.assert_eq[I64](10, r1(0)? as I64)
    h.assert_eq[I64](30, r1(1)? as I64)
    h.assert_eq[I64](50, r1(2)? as I64)

    // Step=1 explicit same as omitted [1:4:1] -> [20, 30, 40]
    let p2 = JsonPathParser.compile("$[1:4:1]")?
    let r2 = p2.query(arr)
    h.assert_eq[USize](3, r2.size())
    h.assert_eq[I64](20, r2(0)? as I64)

    // Negative step: reverse [4:1:-1] -> [50, 40, 30]
    let p3 = JsonPathParser.compile("$[4:1:-1]")?
    let r3 = p3.query(arr)
    h.assert_eq[USize](3, r3.size())
    h.assert_eq[I64](50, r3(0)? as I64)
    h.assert_eq[I64](40, r3(1)? as I64)
    h.assert_eq[I64](30, r3(2)? as I64)

    // Negative step with defaults: reverse entire array [::-1]
    let p4 = JsonPathParser.compile("$[::-1]")?
    let r4 = p4.query(arr)
    h.assert_eq[USize](5, r4.size())
    h.assert_eq[I64](50, r4(0)? as I64)
    h.assert_eq[I64](10, r4(4)? as I64)

    // Step=0 produces no results
    let p5 = JsonPathParser.compile("$[::0]")?
    let r5 = p5.query(arr)
    h.assert_eq[USize](0, r5.size())

    // Negative indices with step: [-4:-1:2] -> [20, 40]
    let p6 = JsonPathParser.compile("$[-4:-1:2]")?
    let r6 = p6.query(arr)
    h.assert_eq[USize](2, r6.size())
    h.assert_eq[I64](20, r6(0)? as I64)
    h.assert_eq[I64](40, r6(1)? as I64)

    // Step with open start/end: [::2] -> [10, 30, 50]
    let p7 = JsonPathParser.compile("$[::2]")?
    let r7 = p7.query(arr)
    h.assert_eq[USize](3, r7.size())
    h.assert_eq[I64](10, r7(0)? as I64)
    h.assert_eq[I64](30, r7(1)? as I64)
    h.assert_eq[I64](50, r7(2)? as I64)

    // Negative step, open start/end: [::-2] -> [50, 30, 10]
    let p8 = JsonPathParser.compile("$[::-2]")?
    let r8 = p8.query(arr)
    h.assert_eq[USize](3, r8.size())
    h.assert_eq[I64](50, r8(0)? as I64)
    h.assert_eq[I64](30, r8(1)? as I64)
    h.assert_eq[I64](10, r8(2)? as I64)

    // Wrong direction: positive step, start > end -> empty
    let p9 = JsonPathParser.compile("$[3:1:1]")?
    let r9 = p9.query(arr)
    h.assert_eq[USize](0, r9.size())

    // Wrong direction: negative step, start < end -> empty
    let p10 = JsonPathParser.compile("$[1:3:-1]")?
    let r10 = p10.query(arr)
    h.assert_eq[USize](0, r10.size())

    // Empty array
    let empty = JsonArray
    let p11 = JsonPathParser.compile("$[::2]")?
    let r11 = p11.query(empty)
    h.assert_eq[USize](0, r11.size())

    // Slice on non-array produces empty result
    let obj = JsonObject.update("a", I64(1))
    let p12 = JsonPathParser.compile("$[::2]")?
    let r12 = p12.query(obj)
    h.assert_eq[USize](0, r12.size())

// ===================================================================
// Example Tests — Token Parser
// ===================================================================

class \nodoc\ iso _TestTokenParserAbort is UnitTest
  fun name(): String => "json/tokenparser/abort"

  fun apply(h: TestHelper) =>
    let parser = JsonTokenParser(
      object is JsonTokenNotify
        var _count: USize = 0
        fun ref apply(parser': JsonTokenParser, token: JsonToken) =>
          _count = _count + 1
          if _count >= 2 then
            parser'.abort()
          end
      end)
    // parse should raise because abort() was called mid-document
    var raised = false
    try
      parser.parse("[1,2,3]")?
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
    _JsonValueStringGen(2)

  fun ref property(sample: String, ph: PropertyHelper) =>
    match \exhaustive\ JsonParser.parse(sample)
    | let doc: JsonValue =>
      let paths: Array[String] val = [
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
          let path = JsonPathParser.compile(path_str)?
          let results = path.query(doc)
          ph.assert_true(results.size() >= 0)
        else
          ph.fail("Failed to compile: " + path_str)
        end
      end
    | let _: JsonParseError => None
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
      _JsonValueStringGen(2),
      _SafeIRegexpGen(1))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json_str, let pattern) = sample
    match \exhaustive\ JsonParser.parse(json_str)
    | let doc: JsonValue =>
      let match_path: String val = "$[?match(@.v, '" + pattern + "')]"
      let search_path: String val = "$[?search(@.v, '" + pattern + "')]"
      match (JsonPathParser.parse(match_path),
        JsonPathParser.parse(search_path))
      | (let mp: JsonPath, let sp: JsonPath) =>
        let match_results = mp.query(doc)
        let search_results = sp.query(doc)
        // Every match result must also be a search result
        ph.assert_true(match_results.size() <= search_results.size(),
          "match returned " + match_results.size().string()
          + " but search returned " + search_results.size().string()
          + " for pattern '" + pattern + "'")
      end
    | let _: JsonParseError => None
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
      _JsonValueStringGen(2),
      _JsonValueStringGen(2))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json1, let json2) = sample
    // Wrap each value inside an array so @.v is always an array.
    // This ensures count(@.v[*]) and length(@.v) both return integers
    // and must agree.
    let wrapped: String val =
      "[{\"v\":[" + json1 + "]},{\"v\":[" + json2 + "]}]"
    match \exhaustive\ JsonParser.parse(wrapped)
    | let doc: JsonValue =>
      match JsonPathParser.parse("$[?count(@.v[*]) == length(@.v)]")
      | let eq_p: JsonPath =>
        let eq_results = eq_p.query(doc)
        // Both elements have array "v", so count and length must agree
        // for both → eq should return 2 results
        ph.assert_eq[USize](2, eq_results.size(),
          "count(@.v[*]) should equal length(@.v) for arrays")
      end
    | let _: JsonParseError => None
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
      _JsonValueStringGen(2),
      _SafeIRegexpGen(1))

  fun ref property(sample: (String, String), ph: PropertyHelper) =>
    (let json_str, let pattern) = sample
    match \exhaustive\ JsonParser.parse(json_str)
    | let doc: JsonValue =>
      let match_path: String val = "$[?match(@.v, '" + pattern + "')]"
      let search_path: String val = "$[?search(@.v, '" + pattern + "')]"
      let not_match: String val = "$[?!match(@.v, '" + pattern + "')]"
      let not_search: String val = "$[?!search(@.v, '" + pattern + "')]"
      let paths: Array[String val] val = [
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
        match \exhaustive\ JsonPathParser.parse(path_str)
        | let path: JsonPath =>
          let results = path.query(doc)
          ph.assert_true(results.size() >= 0)
        | let e: JsonPathParseError =>
          ph.fail("Failed to compile: " + path_str + " — " + e.string())
        end
      end
    | let _: JsonParseError => None
    end

// ===================================================================
// Example Tests — JSONPath Filter Expressions
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterParse is UnitTest
  fun name(): String => "json/jsonpath/filter/parse"

  fun apply(h: TestHelper) =>
    // Valid filter expressions should parse
    let valid: Array[String] val = [
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
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPath => None // pass
      | let e: JsonPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // Invalid filter expressions
    let invalid: Array[String] val = [
      "$[?]"          // empty filter
      "$[?@[*] == 1]" // wildcard in comparison (not singular)
      "$[?@..a == 1]" // descendant in comparison (not singular)
    ]
    for path_str in invalid.values() do
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPathParseError => None // expected
      | let _: JsonPath =>
        h.fail("Expected error for: " + path_str)
      end
    end

class \nodoc\ iso _TestJsonPathFilterExistence is UnitTest
  fun name(): String => "json/jsonpath/filter/existence"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("items", JsonArray
        .push(JsonObject.update("a", I64(1)))
        .push(JsonObject.update("b", I64(2)))
        .push(JsonObject
          .update("a", I64(3))
          .update("b", I64(4))))

    // @.a exists
    let p1 = JsonPathParser.compile("$.items[?@.a]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // @.b exists
    let p2 = JsonPathParser.compile("$.items[?@.b]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](2, r2.size())

    // Negated existence: !@.a
    let p3 = JsonPathParser.compile("$.items[?!@.a]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

    // No items have key "c"
    let p4 = JsonPathParser.compile("$.items[?@.c]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](0, r4.size())

    // Existence with null values — null value still exists
    let doc2 = JsonObject
      .update("items", JsonArray
        .push(JsonObject.update("a", None))
        .push(JsonObject.update("b", I64(1))))
    let p5 = JsonPathParser.compile("$.items[?@.a]")?
    let r5 = p5.query(doc2)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJsonPathFilterComparison is UnitTest
  fun name(): String => "json/jsonpath/filter/comparison"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("store", JsonObject
        .update("book", JsonArray
          .push(JsonObject.update("title", "A").update("price", I64(8)))
          .push(JsonObject.update("title", "B").update("price", I64(12)))
          .push(JsonObject.update("title", "C").update("price", I64(5)))))

    // Less than
    let p1 = JsonPathParser.compile("$.store.book[?@.price < 10]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // Equal
    let p2 = JsonPathParser.compile("$.store.book[?@.price == 12]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    // Not equal
    let p3 = JsonPathParser.compile("$.store.book[?@.price != 12]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

    // Greater than or equal
    let p4 = JsonPathParser.compile("$.store.book[?@.price >= 8]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](2, r4.size())

    // Greater than
    let p5 = JsonPathParser.compile("$.store.book[?@.price > 8]")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](1, r5.size())

    // Less than or equal
    let p6 = JsonPathParser.compile("$.store.book[?@.price <= 5]")?
    let r6 = p6.query(doc)
    h.assert_eq[USize](1, r6.size())

    // String comparison
    let p7 = JsonPathParser.compile(
      """$.store.book[?@.title == "A"]""")?
    let r7 = p7.query(doc)
    h.assert_eq[USize](1, r7.size())

    // Literal on left side
    let p8 = JsonPathParser.compile("$.store.book[?10 > @.price]")?
    let r8 = p8.query(doc)
    h.assert_eq[USize](2, r8.size())

class \nodoc\ iso _TestJsonPathFilterLogical is UnitTest
  fun name(): String => "json/jsonpath/filter/logical"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject.update("a", I64(1)).update("b", I64(2)))
      .push(JsonObject.update("a", I64(3)).update("b", I64(4)))
      .push(JsonObject.update("a", I64(5)).update("b", I64(6)))

    // AND
    let p1 = JsonPathParser.compile("$[?@.a > 1 && @.b < 6]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // OR
    let p2 = JsonPathParser.compile("$[?@.a == 1 || @.a == 5]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](2, r2.size())

    // NOT with parens
    let p3 = JsonPathParser.compile("$[?!(@.a > 3)]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

    // Precedence: && binds tighter than ||
    // @.a == 1 || (@.b == 4 && @.a == 3) -> first and second
    let doc2 = JsonArray
      .push(JsonObject
        .update("a", I64(1)).update("b", I64(2)).update("c", I64(3)))
      .push(JsonObject
        .update("a", I64(4)).update("b", I64(5)).update("c", I64(6)))
      .push(JsonObject
        .update("a", I64(7)).update("b", I64(8)).update("c", I64(9)))

    let p4 = JsonPathParser.compile(
      "$[?@.a == 1 || @.b == 5 && @.c == 6]")?
    let r4 = p4.query(doc2)
    // Parsed as: @.a == 1 || (@.b == 5 && @.c == 6)
    h.assert_eq[USize](2, r4.size())

    // Explicit parens override precedence
    let p5 = JsonPathParser.compile(
      "$[?(@.a == 1 || @.b == 5) && @.c == 6]")?
    let r5 = p5.query(doc2)
    // Only second matches (b==5, c==6)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJsonPathFilterAbsoluteQuery is UnitTest
  fun name(): String => "json/jsonpath/filter/absolute-query"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("default", "X")
      .update("items", JsonArray
        .push(JsonObject.update("type", "X").update("name", "a"))
        .push(JsonObject.update("type", "Y").update("name", "b"))
        .push(JsonObject.update("type", "X").update("name", "c")))

    let p = JsonPathParser.compile("$.items[?@.type == $.default]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

class \nodoc\ iso _TestJsonPathFilterNothing is UnitTest
  fun name(): String => "json/jsonpath/filter/nothing"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject.update("a", I64(1)))
      .push(JsonObject.update("b", I64(2)))
      .push(JsonObject.update("a", None))

    // @.a == 1: only first (Nothing != 1, null != 1)
    let p1 = JsonPathParser.compile("$[?@.a == 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // @.a == null: only third (actual null, not Nothing)
    let p2 = JsonPathParser.compile("$[?@.a == null]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    // @.a != 1: second (Nothing != 1 is true) and third (null != 1 is true)
    let p3 = JsonPathParser.compile("$[?@.a != 1]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](2, r3.size())

class \nodoc\ iso _TestJsonPathFilterTypes is UnitTest
  fun name(): String => "json/jsonpath/filter/types"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject.update("v", I64(1)))
      .push(JsonObject.update("v", "1"))
      .push(JsonObject.update("v", true))
      .push(JsonObject.update("v", None))

    // No type coercion: 1 != "1"
    let p1 = JsonPathParser.compile("$[?@.v == 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    let p2 = JsonPathParser.compile("""$[?@.v == "1"]""")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size())

    let p3 = JsonPathParser.compile("$[?@.v == true]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

    let p4 = JsonPathParser.compile("$[?@.v == null]")?
    let r4 = p4.query(doc)
    h.assert_eq[USize](1, r4.size())

    // String "1" < 2 is false (cross-type)
    let p5 = JsonPathParser.compile("$[?@.v < 2]")?
    let r5 = p5.query(doc)
    h.assert_eq[USize](1, r5.size())

class \nodoc\ iso _TestJsonPathFilterDeepEquality is UnitTest
  fun name(): String => "json/jsonpath/filter/deep-equality"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("target", JsonArray.push(I64(1)).push(I64(2)))
      .update("items", JsonArray
        .push(JsonObject
          .update("v", JsonArray.push(I64(1)).push(I64(2))))
        .push(JsonObject
          .update("v", JsonArray.push(I64(1)).push(I64(3))))
        .push(JsonObject
          .update("v", JsonArray.push(I64(1)).push(I64(2)))))

    let p = JsonPathParser.compile("$.items[?@.v == $.target]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

class \nodoc\ iso _TestJsonPathFilterNumbers is UnitTest
  fun name(): String => "json/jsonpath/filter/numbers"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject.update("v", F64(1.5)))
      .push(JsonObject.update("v", F64(2.5)))
      .push(JsonObject.update("v", F64(3.0)))

    // Float comparison
    let p1 = JsonPathParser.compile("$[?@.v > 2.0]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // Mixed I64/F64: I64(2) == F64(2.0)
    let doc2 = JsonArray
      .push(JsonObject.update("v", I64(2)))
      .push(JsonObject.update("v", I64(3)))

    let p2 = JsonPathParser.compile("$[?@.v == 2.0]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size())

    // Float literal in filter
    let p3 = JsonPathParser.compile("$[?@.v == 3.0]")?
    let r3 = p3.query(doc)
    h.assert_eq[USize](1, r3.size())

class \nodoc\ iso _TestJsonPathFilterOnObjects is UnitTest
  fun name(): String => "json/jsonpath/filter/on-objects"

  fun apply(h: TestHelper) ? =>
    let doc = JsonObject
      .update("data", JsonObject
        .update("x", JsonObject.update("active", true))
        .update("y", JsonObject.update("active", false))
        .update("z", JsonObject.update("active", true)))

    let p = JsonPathParser.compile("$.data[?@.active == true]")?
    let r = p.query(doc)
    h.assert_eq[USize](2, r.size())

class \nodoc\ iso _TestJsonPathFilterNested is UnitTest
  fun name(): String => "json/jsonpath/filter/nested"

  fun apply(h: TestHelper) ? =>
    // Nested filter: outer filter with inner bracket access
    let doc = JsonArray
      .push(JsonObject
        .update("items", JsonArray.push(I64(1)).push(I64(5))))
      .push(JsonObject
        .update("items", JsonArray.push(I64(10)).push(I64(20))))

    // Select elements where items[0] > 5
    let p1 = JsonPathParser.compile("$[?@.items[0] > 5]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size())

    // $ reference inside filter resolves to document root
    let doc2 = JsonObject
      .update("threshold", I64(3))
      .update("items", JsonArray
        .push(JsonObject.update("v", I64(1)))
        .push(JsonObject.update("v", I64(5)))
        .push(JsonObject.update("v", I64(3))))

    let p2 = JsonPathParser.compile("$.items[?@.v > $.threshold]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size())

// ===================================================================
// Example Tests — JSONPath Function Extension Parse
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterFunctionParse is UnitTest
  fun name(): String => "json/jsonpath/filter/function/parse"

  fun apply(h: TestHelper) =>
    // Valid function expressions
    let valid: Array[String] val = [
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
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPath => None
      | let e: JsonPathParseError =>
        h.fail("Expected valid: " + path_str + " — " + e.string())
      end
    end

    // Invalid function expressions
    let invalid: Array[(String, String)] val = [
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
      match \exhaustive\ JsonPathParser.parse(path_str)
      | let _: JsonPathParseError => None
      | let _: JsonPath =>
        h.fail("Expected error for (" + label + "): " + path_str)
      end
    end

// ===================================================================
// Example Tests — JSONPath Function Extension match/search
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterFunctionMatchSearch is UnitTest
  fun name(): String => "json/jsonpath/filter/function/match-search"

  fun apply(h: TestHelper) ? =>
    // RFC 9535 Section 2.4.6/2.4.7 example
    let doc = JsonObject
      .update("a", JsonArray
        .push(I64(3))
        .push(I64(5))
        .push(I64(1))
        .push(I64(2))
        .push(I64(4))
        .push(I64(6))
        .push(JsonObject.update("b", "j"))
        .push(JsonObject.update("b", "k"))
        .push(JsonObject.update("b", JsonObject))
        .push(JsonObject.update("b", "kilo")))

    // match: full-string only — "j" and "k" match [jk], "kilo" does not
    let p1 = JsonPathParser.compile("""$.a[?match(@.b, "[jk]")]""")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](2, r1.size())

    // search: substring — "j", "k", and "kilo" all contain [jk]
    let p2 = JsonPathParser.compile("""$.a[?search(@.b, "[jk]")]""")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](3, r2.size())

    // Negated match
    let doc2 = JsonArray
      .push(JsonObject.update("name", "alice"))
      .push(JsonObject.update("name", "bob"))
      .push(JsonObject.update("name", "carol"))
    let p3 = JsonPathParser.compile("""$[?!match(@.name, "b.*")]""")?
    let r3 = p3.query(doc2)
    h.assert_eq[USize](2, r3.size())

    // Non-string args → false (numbers don't match)
    let doc3 = JsonArray
      .push(JsonObject.update("v", I64(42)))
      .push(JsonObject.update("v", "hello"))
    let p4 = JsonPathParser.compile("""$[?match(@.v, ".*")]""")?
    let r4 = p4.query(doc3)
    // Only "hello" matches — I64(42) is not a string
    h.assert_eq[USize](1, r4.size())

    // Invalid regex pattern → false (not crash)
    let doc4 = JsonArray
      .push(JsonObject.update("v", "test"))
    let p5 = JsonPathParser.compile("""$[?match(@.v, "[invalid")]""")?
    let r5 = p5.query(doc4)
    h.assert_eq[USize](0, r5.size())

    // match vs search: "abc" matches "abc" fully, "xabcx" does not
    let doc5 = JsonArray
      .push(JsonObject.update("v", "abc"))
      .push(JsonObject.update("v", "xabcx"))
    let p6 = JsonPathParser.compile("""$[?match(@.v, "abc")]""")?
    let r6 = p6.query(doc5)
    h.assert_eq[USize](1, r6.size()) // only exact "abc"

    let p7 = JsonPathParser.compile("""$[?search(@.v, "abc")]""")?
    let r7 = p7.query(doc5)
    h.assert_eq[USize](2, r7.size()) // both contain "abc"

// ===================================================================
// Example Tests — JSONPath Function Extension length
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterFunctionLength is UnitTest
  fun name(): String => "json/jsonpath/filter/function/length"

  fun apply(h: TestHelper) ? =>
    // String length — ASCII
    let doc1 = JsonArray
      .push(JsonObject.update("name", "Al"))
      .push(JsonObject.update("name", "Bob"))
      .push(JsonObject.update("name", "Carol"))
    let p1 = JsonPathParser.compile("$[?length(@.name) <= 3]")?
    let r1 = p1.query(doc1)
    h.assert_eq[USize](2, r1.size()) // "Al" (2) and "Bob" (3)

    // String length — Unicode multi-byte: "café" has 4 codepoints not 5 bytes
    let cafe: String val = recover val
      String.>append("caf").>push(0xC3).>push(0xA9)
    end
    let doc2 = JsonArray
      .push(JsonObject.update("s", cafe))
      .push(JsonObject.update("s", "hello"))
    let p2 = JsonPathParser.compile("$[?length(@.s) == 4]")?
    let r2 = p2.query(doc2)
    h.assert_eq[USize](1, r2.size()) // only "café"

    // Array size
    let doc3 = JsonArray
      .push(JsonObject.update("items", JsonArray.push(I64(1)).push(I64(2))))
      .push(JsonObject.update("items", JsonArray.push(I64(1))))
    let p3 = JsonPathParser.compile("$[?length(@.items) > 1]")?
    let r3 = p3.query(doc3)
    h.assert_eq[USize](1, r3.size())

    // Object member count
    let doc4 = JsonArray
      .push(JsonObject.update("obj",
        JsonObject.update("a", I64(1)).update("b", I64(2)).update("c", I64(3))))
      .push(JsonObject.update("obj",
        JsonObject.update("x", I64(1))))
    let p4 = JsonPathParser.compile("$[?length(@.obj) >= 3]")?
    let r4 = p4.query(doc4)
    h.assert_eq[USize](1, r4.size())

    // length on number/bool/null/Nothing → Nothing, comparison fails → 0 matches
    let doc5 = JsonArray
      .push(JsonObject.update("v", I64(42)))
      .push(JsonObject.update("v", true))
      .push(JsonObject.update("v", None))
      .push(JsonObject) // missing "v" → Nothing
    let p5 = JsonPathParser.compile("$[?length(@.v) > 0]")?
    let r5 = p5.query(doc5)
    h.assert_eq[USize](0, r5.size())

    // Nested: length(value(@.items))
    let doc6 = JsonObject
      .update("items", JsonArray
        .push(JsonObject.update("x", "abc"))
        .push(JsonObject.update("x", "abcde")))
    let p6 = JsonPathParser.compile("$.items[?length(value(@.x)) > 3]")?
    let r6 = p6.query(doc6)
    h.assert_eq[USize](1, r6.size()) // only "abcde" (length 5)

// ===================================================================
// Example Tests — JSONPath Function Extension count
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterFunctionCount is UnitTest
  fun name(): String => "json/jsonpath/filter/function/count"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject
        .update("items", JsonArray.push(I64(1)).push(I64(2)).push(I64(3))))
      .push(JsonObject
        .update("items", JsonArray.push(I64(1))))
      .push(JsonObject
        .update("items", JsonArray))

    // count(@.items[*]) counts array elements
    let p1 = JsonPathParser.compile("$[?count(@.items[*]) > 1]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size()) // only first (3 items)

    // count(@.items[*]) == 0 for empty array
    let p2 = JsonPathParser.compile("$[?count(@.items[*]) == 0]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](1, r2.size()) // third element

    // count on object members via wildcard
    let doc2 = JsonArray
      .push(JsonObject.update("data",
        JsonObject.update("a", I64(1)).update("b", I64(2))))
      .push(JsonObject.update("data",
        JsonObject.update("x", I64(1))))
    let p3 = JsonPathParser.compile("$[?count(@.data.*) > 1]")?
    let r3 = p3.query(doc2)
    h.assert_eq[USize](1, r3.size())

    // count on non-container via wildcard → 0
    let doc3 = JsonArray
      .push(JsonObject.update("v", I64(42)))
    let p4 = JsonPathParser.compile("$[?count(@.v.*) > 0]")?
    let r4 = p4.query(doc3)
    h.assert_eq[USize](0, r4.size())

// ===================================================================
// Example Tests — JSONPath Function Extension value
// ===================================================================

class \nodoc\ iso _TestJsonPathFilterFunctionValue is UnitTest
  fun name(): String => "json/jsonpath/filter/function/value"

  fun apply(h: TestHelper) ? =>
    let doc = JsonArray
      .push(JsonObject
        .update("items", JsonArray.push(I64(10)).push(I64(20))))
      .push(JsonObject
        .update("items", JsonArray.push(I64(5))))
      .push(JsonObject
        .update("items", JsonArray))

    // value(@.items[0]) extracts single element
    let p1 = JsonPathParser.compile("$[?value(@.items[0]) > 7]")?
    let r1 = p1.query(doc)
    h.assert_eq[USize](1, r1.size()) // only first (10 > 7)

    // value on empty query → Nothing → comparison fails
    let p2 = JsonPathParser.compile("$[?value(@.missing) == 1]")?
    let r2 = p2.query(doc)
    h.assert_eq[USize](0, r2.size())

    // value on multi-element wildcard → Nothing
    let p3 = JsonPathParser.compile("$[?value(@.items[*]) == 5]")?
    let r3 = p3.query(doc)
    // First has 2 items (Nothing), second has 1 item (5), third has 0 (Nothing)
    h.assert_eq[USize](1, r3.size()) // only second

    // value must return Nothing for multi-element result, even if first matches
    let doc_multi = JsonArray
      .push(JsonObject
        .update("items", JsonArray.push(I64(5)).push(I64(99))))
    let p_multi = JsonPathParser.compile("$[?value(@.items[*]) == 5]")?
    let r_multi = p_multi.query(doc_multi)
    // 2 items → value returns Nothing, not 5
    h.assert_eq[USize](0, r_multi.size())

    // value used with string comparison
    let doc2 = JsonArray
      .push(JsonObject.update("tags", JsonArray.push("urgent")))
      .push(JsonObject.update("tags", JsonArray.push("low")))
    let p4 = JsonPathParser.compile(
      """$[?value(@.tags[0]) == "urgent"]""")?
    let r4 = p4.query(doc2)
    h.assert_eq[USize](1, r4.size())

