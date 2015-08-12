use "ponytest"
use "collections"


actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestParseBasic)
    test(_TestParseKeyword)
    test(_TestParseNumber)
    test(_TestParseString)
    test(_TestParseArray)
    test(_TestParseObject)
    test(_TestParseRFC1)
    test(_TestParseRFC2)

    test(_TestPrintKeyword)
    test(_TestPrintNumber)
    test(_TestPrintString)
    test(_TestPrintArray)
    test(_TestPrintObject)
    
    test(_TestParsePrint)


class _TestParseBasic iso is UnitTest
  """
  Test Json basic parsing, eg allowing whitespace.
  """
  fun name(): String => "JSON/parse.basic"

  fun apply(h: TestHelper): TestResult ? =>
    var doc = JsonParser.parse("true")
    h.expect_eq[Bool](true, doc.data as Bool)
    
    doc = JsonParser.parse(" true   ")
    h.expect_eq[Bool](true, doc.data as Bool)

    h.expect_error(lambda()? => JsonParser.parse("") end)
    h.expect_error(lambda()? => JsonParser.parse("   ") end)
    h.expect_error(lambda()? => JsonParser.parse("true true") end)

    true


class _TestParseKeyword iso is UnitTest
  """
  Test Json parsing of keywords.
  """
  fun name(): String => "JSON/parse.keyword"

  fun apply(h: TestHelper): TestResult ? =>
    var doc = JsonParser.parse("true")
    h.expect_eq[Bool](true, doc.data as Bool)
    
    doc = JsonParser.parse("false")
    h.expect_eq[Bool](false, doc.data as Bool)
    
    doc = JsonParser.parse("null")
    h.expect_eq[None](None, doc.data as None)

    h.expect_error(lambda()? => JsonParser.parse("tru e") end)
    h.expect_error(lambda()? => JsonParser.parse("truw") end)
    h.expect_error(lambda()? => JsonParser.parse("truez") end)
    h.expect_error(lambda()? => JsonParser.parse("TRUE") end)

    true


class _TestParseNumber iso is UnitTest
  """
  Test Json parsing of numbers.
  """
  fun name(): String => "JSON/parse.number"

  fun apply(h: TestHelper): TestResult ? =>
    var doc = JsonParser.parse("0")
    h.expect_eq[I64](0, doc.data as I64)

    doc = JsonParser.parse("13")
    h.expect_eq[I64](13, doc.data as I64)
    
    doc = JsonParser.parse("-13")
    h.expect_eq[I64](-13, doc.data as I64)
    
    doc = JsonParser.parse("1.5")
    h.expect_eq[F64](1.5, doc.data as F64)
    
    doc = JsonParser.parse("-1.125")
    h.expect_eq[F64](-1.125, doc.data as F64)
    
    doc = JsonParser.parse("1e3")
    h.expect_eq[F64](1000, doc.data as F64)
    
    doc = JsonParser.parse("1e+3")
    h.expect_eq[F64](1000, doc.data as F64)
    
    doc = JsonParser.parse("1e-3")
    h.expect_eq[F64](0.001, doc.data as F64)
    
    doc = JsonParser.parse("1.23e3")
    h.expect_eq[F64](1230, doc.data as F64)
    
    h.expect_error(lambda()? => JsonParser.parse("0x4") end)
    h.expect_error(lambda()? => JsonParser.parse("+1") end)
    h.expect_error(lambda()? => JsonParser.parse("1.") end)
    h.expect_error(lambda()? => JsonParser.parse("1.-3") end)
    h.expect_error(lambda()? => JsonParser.parse("1e") end)

    true


class _TestParseString iso is UnitTest
  """
  Test Json parsing of strings.
  """
  fun name(): String => "JSON/parse.string"

  fun apply(h: TestHelper): TestResult ? =>
    var doc = JsonParser.parse(""""Foo"""")
    h.expect_eq[String]("Foo", doc.data as String)

    doc = JsonParser.parse(""" "Foo\tbar" """)
    h.expect_eq[String]("Foo\tbar", doc.data as String)

    doc = JsonParser.parse(""" "Foo\"bar" """)
    h.expect_eq[String]("""Foo"bar""", doc.data as String)

    doc = JsonParser.parse(""" "Foo\\bar" """)
    h.expect_eq[String]("""Foo\bar""", doc.data as String)

    doc = JsonParser.parse(""" "Foo\u0020bar" """)
    h.expect_eq[String]("""Foo bar""", doc.data as String)

    doc = JsonParser.parse(""" "Foo\u004Fbar" """)
    h.expect_eq[String]("""FooObar""", doc.data as String)

    doc = JsonParser.parse(""" "Foo\u004fbar" """)
    h.expect_eq[String]("""FooObar""", doc.data as String)

    doc = JsonParser.parse(""" "Foo\uD834\uDD1Ebar" """)
    h.expect_eq[String]("Foo\U01D11Ebar", doc.data as String)
    
    h.expect_error(lambda()? => JsonParser.parse(""" "Foo """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "Foo\z" """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "\u" """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "\u37" """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "\u037g" """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "\uD834" """) end)
    h.expect_error(lambda()? => JsonParser.parse(""" "\uDD1E" """) end)

    true


class _TestParseArray iso is UnitTest
  """
  Test Json parsing of arrays.
  """
  fun name(): String => "JSON/parse.array"

  fun apply(h: TestHelper): TestResult ? =>
    var doc: JsonDoc ref = JsonParser.parse("[]")
    h.expect_eq[U64](0, (doc.data as JsonArray).data.size())

    doc = JsonParser.parse("[ ]")
    h.expect_eq[U64](0, (doc.data as JsonArray).data.size())
    
    doc = JsonParser.parse("[true]")
    h.expect_eq[U64](1, (doc.data as JsonArray).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    
    doc = JsonParser.parse("[ true ]")
    h.expect_eq[U64](1, (doc.data as JsonArray).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    
    doc = JsonParser.parse("[true, false]")
    h.expect_eq[U64](2, (doc.data as JsonArray).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.expect_eq[Bool](false, (doc.data as JsonArray).data(1) as Bool)
    
    doc = JsonParser.parse("[true , 13,null]")
    h.expect_eq[U64](3, (doc.data as JsonArray).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.expect_eq[I64](13, (doc.data as JsonArray).data(1) as I64)
    h.expect_eq[None](None, (doc.data as JsonArray).data(2) as None)
    
    doc = JsonParser.parse("[true, [52, null]]")
    h.expect_eq[U64](2, (doc.data as JsonArray).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.expect_eq[U64](2,
      ((doc.data as JsonArray).data(1) as JsonArray).data.size())
    h.expect_eq[I64](52,
      ((doc.data as JsonArray).data(1) as JsonArray).data(0) as I64)
    h.expect_eq[None](None,
      ((doc.data as JsonArray).data(1) as JsonArray).data(1) as None)
    
    h.expect_error(lambda()? => JsonParser.parse("[true true]") end)
    h.expect_error(lambda()? => JsonParser.parse("[,]") end)
    h.expect_error(lambda()? => JsonParser.parse("[true,]") end)
    h.expect_error(lambda()? => JsonParser.parse("[,true]") end)
    h.expect_error(lambda()? => JsonParser.parse("[") end)
    h.expect_error(lambda()? => JsonParser.parse("[true") end)
    h.expect_error(lambda()? => JsonParser.parse("[true,") end)

    true


class _TestParseObject iso is UnitTest
  """
  Test Json parsing of objects.
  """
  fun name(): String => "JSON/parse.object"

  fun apply(h: TestHelper): TestResult ? =>
    var doc: JsonDoc ref = JsonParser.parse("{}")
    h.expect_eq[U64](0, (doc.data as JsonObject).data.size())

    doc = JsonParser.parse("{ }")
    h.expect_eq[U64](0, (doc.data as JsonObject).data.size())
    
    doc = JsonParser.parse("""{"foo": true}""")
    h.expect_eq[U64](1, (doc.data as JsonObject).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonObject).data("foo") as Bool)
    
    doc = JsonParser.parse("""{ "foo" :"true" }""")
    h.expect_eq[U64](1, (doc.data as JsonObject).data.size())
    h.expect_eq[String]("true", (doc.data as JsonObject).data("foo") as String)
    
    doc = JsonParser.parse("""{"a": true, "b": false}""")
    h.expect_eq[U64](2, (doc.data as JsonObject).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.expect_eq[Bool](false, (doc.data as JsonObject).data("b") as Bool)
    
    doc = JsonParser.parse("""{"a": true , "b": 13,"c":null}""")
    h.expect_eq[U64](3, (doc.data as JsonObject).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.expect_eq[I64](13, (doc.data as JsonObject).data("b") as I64)
    h.expect_eq[None](None, (doc.data as JsonObject).data("c") as None)
    
    doc = JsonParser.parse("""{"a": true, "b": {"c": 52, "d": null}}""")
    h.expect_eq[U64](2, (doc.data as JsonObject).data.size())
    h.expect_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.expect_eq[U64](2,
      ((doc.data as JsonObject).data("b") as JsonObject).data.size())
    h.expect_eq[I64](52,
      ((doc.data as JsonObject).data("b") as JsonObject).data("c") as I64)
    h.expect_eq[None](None,
      ((doc.data as JsonObject).data("b") as JsonObject).data("d") as None)
    
    h.expect_error(lambda()? => JsonParser.parse("""{"a": 1 "b": 2}""") end)
    h.expect_error(lambda()? => JsonParser.parse("{,}") end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a": true,}""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{,"a": true}""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a" """) end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a": """) end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a": true""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a": true,""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{"a" true}""") end)
    h.expect_error(lambda()? => JsonParser.parse("""{:true}""") end)

    true


class _TestParseRFC1 iso is UnitTest
  """
  Test Json parsing of first example from RFC7159.
  """
  fun name(): String => "JSON/parse.rfc1"

  fun apply(h: TestHelper): TestResult ? =>
    let src =
      """
      {
        "Image": {
          "Width":  800,
          "Height": 600,
          "Title":  "View from 15th Floor",
          "Thumbnail": {
            "Url":    "http://www.example.com/image/481989943",
            "Height": 125,
            "Width":  100
          },
          "Animated" : false,
          "IDs": [116, 943, 234, 38793]
        }
      }
      """

    let doc: JsonDoc ref = JsonParser.parse(src)
    let obj1 = doc.data as JsonObject

    h.expect_eq[U64](1, obj1.data.size())

    let obj2 = obj1.data("Image") as JsonObject

    h.expect_eq[U64](6, obj2.data.size())
    h.expect_eq[I64](800, obj2.data("Width") as I64)
    h.expect_eq[I64](600, obj2.data("Height") as I64)
    h.expect_eq[String]("View from 15th Floor", obj2.data("Title") as String)
    h.expect_eq[Bool](false, obj2.data("Animated") as Bool)

    let obj3 = obj2.data("Thumbnail") as JsonObject

    h.expect_eq[U64](3, obj3.data.size())
    h.expect_eq[I64](100, obj3.data("Width") as I64)
    h.expect_eq[I64](125, obj3.data("Height") as I64)
    h.expect_eq[String]("http://www.example.com/image/481989943",
      obj3.data("Url") as String)

    let array = obj2.data("IDs") as JsonArray
    
    h.expect_eq[U64](4, array.data.size())
    h.expect_eq[I64](116, array.data(0) as I64)
    h.expect_eq[I64](943, array.data(1) as I64)
    h.expect_eq[I64](234, array.data(2) as I64)
    h.expect_eq[I64](38793, array.data(3) as I64)

    true


class _TestParseRFC2 iso is UnitTest
  """
  Test Json parsing of second example from RFC7159.
  """
  fun name(): String => "JSON/parse.rfc2"

  fun apply(h: TestHelper): TestResult ? =>
    let src =
      """
      [
        {
          "precision": "zip",
          "Latitude":  37.7668,
          "Longitude": -122.3959,
          "Address":   "",
          "City":      "SAN FRANCISCO",
          "State":     "CA",
          "Zip":       "94107",
          "Country":   "US"
        },
        {
          "precision": "zip",
          "Latitude":  37.371991,
          "Longitude": -122.026020,
          "Address":   "",
          "City":      "SUNNYVALE",
          "State":     "CA",
          "Zip":       "94085",
          "Country":   "US"
        }
      ]
      """

    let doc: JsonDoc ref = JsonParser.parse(src)
    let array = doc.data as JsonArray

    h.expect_eq[U64](2, array.data.size())

    let obj1 = array.data(0) as JsonObject

    h.expect_eq[U64](8, obj1.data.size())
    h.expect_eq[String]("zip", obj1.data("precision") as String)
    h.expect_true(((obj1.data("Latitude") as F64) - 37.7668).abs() < 0.001)
    h.expect_true(((obj1.data("Longitude") as F64) + 122.3959).abs() < 0.001)
    h.expect_eq[String]("", obj1.data("Address") as String)
    h.expect_eq[String]("SAN FRANCISCO", obj1.data("City") as String)
    h.expect_eq[String]("CA", obj1.data("State") as String)
    h.expect_eq[String]("94107", obj1.data("Zip") as String)
    h.expect_eq[String]("US", obj1.data("Country") as String)

    let obj2 = array.data(1) as JsonObject

    h.expect_eq[U64](8, obj2.data.size())
    h.expect_eq[String]("zip", obj2.data("precision") as String)
    h.expect_true(((obj2.data("Latitude") as F64) - 37.371991).abs() < 0.001)
    h.expect_true(((obj2.data("Longitude") as F64) + 122.026020).abs() < 0.001)
    h.expect_eq[String]("", obj2.data("Address") as String)
    h.expect_eq[String]("SUNNYVALE", obj2.data("City") as String)
    h.expect_eq[String]("CA", obj2.data("State") as String)
    h.expect_eq[String]("94085", obj2.data("Zip") as String)
    h.expect_eq[String]("US", obj2.data("Country") as String)

    true


class _TestPrintKeyword iso is UnitTest
  """
  Test Json printing of keywords.
  """
  fun name(): String => "JSON/print.keyword"

  fun apply(h: TestHelper): TestResult =>
    var doc = JsonDoc

    doc.data = true
    h.expect_eq[String]("true", doc.string())

    doc.data = false
    h.expect_eq[String]("false", doc.string())

    doc.data = None
    h.expect_eq[String]("null", doc.string())

    true


class _TestPrintNumber iso is UnitTest
  """
  Test Json printing of numbers.
  """
  fun name(): String => "JSON/print.number"

  fun apply(h: TestHelper): TestResult =>
    var doc = JsonDoc

    doc.data = I64(0)
    h.expect_eq[String]("0", doc.string())

    doc.data = I64(13)
    h.expect_eq[String]("13", doc.string())

    doc.data = I64(-13)
    h.expect_eq[String]("-13", doc.string())

    doc.data = F64(0)
    h.expect_eq[String]("0.0", doc.string())

    doc.data = F64(1.5)
    h.expect_eq[String]("1.5", doc.string())

    doc.data = F64(-1.5)
    h.expect_eq[String]("-1.5", doc.string())

    // We don't test exponent formatted output because it can be slightly
    // different on different platforms
    true


class _TestPrintString iso is UnitTest
  """
  Test Json printing of strings.
  """
  fun name(): String => "JSON/print.string"

  fun apply(h: TestHelper): TestResult =>
    var doc = JsonDoc

    doc.data = "Foo"
    h.expect_eq[String](""""Foo"""", doc.string())

    doc.data = "Foo\tbar"
    h.expect_eq[String](""""Foo\tbar"""", doc.string())

    doc.data = "Foo\"bar"
    h.expect_eq[String](""""Foo\"bar"""", doc.string())

    doc.data = "Foo\\bar"
    h.expect_eq[String](""""Foo\\bar"""", doc.string())

    doc.data = "Foo\abar"
    h.expect_eq[String](""""Foo\u0007bar"""", doc.string())

    doc.data = "Foo\u1000bar"
    h.expect_eq[String](""""Foo\u1000bar"""", doc.string())

    doc.data = "Foo\U01D11Ebar"
    h.expect_eq[String](""""Foo\uD834\uDD1Ebar"""", doc.string())

    true


class _TestPrintArray iso is UnitTest
  """
  Test Json printing of arrays.
  """
  fun name(): String => "JSON/print.array"

  fun apply(h: TestHelper): TestResult =>
    let doc: JsonDoc = JsonDoc
    let array: JsonArray = JsonArray

    doc.data = array
    h.expect_eq[String]("[]", doc.string())

    array.data.clear()
    array.data.push(true)
    h.expect_eq[String]("[\n  true\n]", doc.string())
    
    array.data.clear()
    array.data.push(true)
    array.data.push(false)
    h.expect_eq[String]("[\n  true,\n  false\n]", doc.string())
    
    array.data.clear()
    array.data.push(true)
    array.data.push(I64(13))
    array.data.push(None)
    h.expect_eq[String]("[\n  true,\n  13,\n  null\n]", doc.string())
    
    array.data.clear()
    array.data.push(true)
    var nested: JsonArray = JsonArray
    nested.data.push(I64(52))
    nested.data.push(None)
    array.data.push(nested)
    h.expect_eq[String]("[\n  true,\n  [\n    52,\n    null\n  ]\n]",
      doc.string())

    true


class _TestPrintObject iso is UnitTest
  """
  Test Json printing of objects.
  """
  fun name(): String => "JSON/print.object"

  fun apply(h: TestHelper): TestResult =>
    let doc: JsonDoc = JsonDoc
    let obj: JsonObject = JsonObject

    doc.data = obj
    h.expect_eq[String]("{}", doc.string())
    
    obj.data.clear()
    obj.data("foo") = true
    h.expect_eq[String]("{\n  \"foo\": true\n}", doc.string())
    
    obj.data.clear()
    obj.data("a") = true
    obj.data("b") = I64(3)
    let s = doc.string()
    h.expect_true((s == "{\n  \"a\": true,\n  \"b\": 3\n}") or
      (s == "{\n  \"b\": false,\n  \"a\": true\n}"))

    // We don't test with more fields in the object because we don't know what
    // order they will be printed in
    true


class _TestParsePrint iso is UnitTest
  """
  Test Json parsing a complex example and then reprinting it.
  """
  fun name(): String => "JSON/parseprint"

  fun apply(h: TestHelper): TestResult ? =>
    // Note that this example contains no objects with more than 1 member,
    // because the order the fields are printed is unpredictable
    let src =
      """
      [
        {
          "precision": "zip"
        },
        {
          "data": [
            {},
            "Really?",
            "yes",
            true,
            4,
            12.3
          ]
        },
        47,
        {
          "foo": {
            "bar": [
              {
                "aardvark": null
              },
              false
            ]
          }
        }
      ]"""

    let doc: JsonDoc ref = JsonParser.parse(src)
    let printed = doc.string()

    // TODO: Sort out line endings on different platforms. For now normalise
    // before comparing 
    let actual: String ref = printed.clone()
    actual.remove("\r")

    let expect: String ref = src.clone()
    expect.remove("\r")

    h.expect_eq[String ref](expect, actual)

    true
