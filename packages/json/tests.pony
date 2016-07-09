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

    test(_TestNoPrettyPrintArray)
    test(_TestNoPrettyPrintObject)

    test(_TestParsePrint)


class iso _TestParseBasic is UnitTest
  """
  Test Json basic parsing, eg allowing whitespace.
  """
  fun name(): String => "JSON/parse.basic"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse("true")
    h.assert_eq[Bool](true, doc.data as Bool)

    doc.parse(" true   ")
    h.assert_eq[Bool](true, doc.data as Bool)

    h.assert_error(lambda()? => JsonDoc.parse("") end)
    h.assert_error(lambda()? => JsonDoc.parse("   ") end)
    h.assert_error(lambda()? => JsonDoc.parse("true true") end)


class iso _TestParseKeyword is UnitTest
  """
  Test Json parsing of keywords.
  """
  fun name(): String => "JSON/parse.keyword"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse("true")
    h.assert_eq[Bool](true, doc.data as Bool)

    doc.parse("false")
    h.assert_eq[Bool](false, doc.data as Bool)

    doc.parse("null")
    h.assert_eq[None](None, doc.data as None)

    h.assert_error(lambda()? => JsonDoc.parse("tru e") end)
    h.assert_error(lambda()? => JsonDoc.parse("truw") end)
    h.assert_error(lambda()? => JsonDoc.parse("truez") end)
    h.assert_error(lambda()? => JsonDoc.parse("TRUE") end)


class iso _TestParseNumber is UnitTest
  """
  Test Json parsing of numbers.
  """
  fun name(): String => "JSON/parse.number"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse("0")
    h.assert_eq[I64](0, doc.data as I64)

    doc.parse("13")
    h.assert_eq[I64](13, doc.data as I64)

    doc.parse("-13")
    h.assert_eq[I64](-13, doc.data as I64)

    doc.parse("1.5")
    h.assert_eq[F64](1.5, doc.data as F64)

    doc.parse("-1.125")
    h.assert_eq[F64](-1.125, doc.data as F64)

    doc.parse("1e3")
    h.assert_eq[F64](1000, doc.data as F64)

    doc.parse("1e+3")
    h.assert_eq[F64](1000, doc.data as F64)

    doc.parse("1e-3")
    h.assert_eq[F64](0.001, doc.data as F64)

    doc.parse("1.23e3")
    h.assert_eq[F64](1230, doc.data as F64)

    h.assert_error(lambda()? => JsonDoc.parse("0x4") end)
    h.assert_error(lambda()? => JsonDoc.parse("+1") end)
    h.assert_error(lambda()? => JsonDoc.parse("1.") end)
    h.assert_error(lambda()? => JsonDoc.parse("1.-3") end)
    h.assert_error(lambda()? => JsonDoc.parse("1e") end)


class iso _TestParseString is UnitTest
  """
  Test Json parsing of strings.
  """
  fun name(): String => "JSON/parse.string"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse(""""Foo"""")
    h.assert_eq[String]("Foo", doc.data as String)

    doc.parse(""" "Foo\tbar" """)
    h.assert_eq[String]("Foo\tbar", doc.data as String)

    doc.parse(""" "Foo\"bar" """)
    h.assert_eq[String]("""Foo"bar""", doc.data as String)

    doc.parse(""" "Foo\\bar" """)
    h.assert_eq[String]("""Foo\bar""", doc.data as String)

    doc.parse(""" "Foo\u0020bar" """)
    h.assert_eq[String]("""Foo bar""", doc.data as String)

    doc.parse(""" "Foo\u004Fbar" """)
    h.assert_eq[String]("""FooObar""", doc.data as String)

    doc.parse(""" "Foo\u004fbar" """)
    h.assert_eq[String]("""FooObar""", doc.data as String)

    doc.parse(""" "Foo\uD834\uDD1Ebar" """)
    h.assert_eq[String]("Foo\U01D11Ebar", doc.data as String)

    h.assert_error(lambda()? => JsonDoc.parse(""" "Foo """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "Foo\z" """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "\u" """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "\u37" """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "\u037g" """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "\uD834" """) end)
    h.assert_error(lambda()? => JsonDoc.parse(""" "\uDD1E" """) end)


class iso _TestParseArray is UnitTest
  """
  Test Json parsing of arrays.
  """
  fun name(): String => "JSON/parse.array"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse("[]")
    h.assert_eq[USize](0, (doc.data as JsonArray).data.size())

    doc.parse("[ ]")
    h.assert_eq[USize](0, (doc.data as JsonArray).data.size())

    doc.parse("[true]")
    h.assert_eq[USize](1, (doc.data as JsonArray).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)

    doc.parse("[ true ]")
    h.assert_eq[USize](1, (doc.data as JsonArray).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)

    doc.parse("[true, false]")
    h.assert_eq[USize](2, (doc.data as JsonArray).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.assert_eq[Bool](false, (doc.data as JsonArray).data(1) as Bool)

    doc.parse("[true , 13,null]")
    h.assert_eq[USize](3, (doc.data as JsonArray).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.assert_eq[I64](13, (doc.data as JsonArray).data(1) as I64)
    h.assert_eq[None](None, (doc.data as JsonArray).data(2) as None)

    doc.parse("[true, [52, null]]")
    h.assert_eq[USize](2, (doc.data as JsonArray).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonArray).data(0) as Bool)
    h.assert_eq[USize](2,
      ((doc.data as JsonArray).data(1) as JsonArray).data.size())
    h.assert_eq[I64](52,
      ((doc.data as JsonArray).data(1) as JsonArray).data(0) as I64)
    h.assert_eq[None](None,
      ((doc.data as JsonArray).data(1) as JsonArray).data(1) as None)

    h.assert_error(lambda()? => JsonDoc.parse("[true true]") end)
    h.assert_error(lambda()? => JsonDoc.parse("[,]") end)
    h.assert_error(lambda()? => JsonDoc.parse("[true,]") end)
    h.assert_error(lambda()? => JsonDoc.parse("[,true]") end)
    h.assert_error(lambda()? => JsonDoc.parse("[") end)
    h.assert_error(lambda()? => JsonDoc.parse("[true") end)
    h.assert_error(lambda()? => JsonDoc.parse("[true,") end)


class iso _TestParseObject is UnitTest
  """
  Test Json parsing of objects.
  """
  fun name(): String => "JSON/parse.object"

  fun apply(h: TestHelper) ? =>
    let doc: JsonDoc = JsonDoc

    doc.parse("{}")
    h.assert_eq[USize](0, (doc.data as JsonObject).data.size())

    doc.parse("{ }")
    h.assert_eq[USize](0, (doc.data as JsonObject).data.size())

    doc.parse("""{"foo": true}""")
    h.assert_eq[USize](1, (doc.data as JsonObject).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonObject).data("foo") as Bool)

    doc.parse("""{ "foo" :"true" }""")
    h.assert_eq[USize](1, (doc.data as JsonObject).data.size())
    h.assert_eq[String]("true", (doc.data as JsonObject).data("foo") as String)

    doc.parse("""{"a": true, "b": false}""")
    h.assert_eq[USize](2, (doc.data as JsonObject).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.assert_eq[Bool](false, (doc.data as JsonObject).data("b") as Bool)

    doc.parse("""{"a": true , "b": 13,"c":null}""")
    h.assert_eq[USize](3, (doc.data as JsonObject).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.assert_eq[I64](13, (doc.data as JsonObject).data("b") as I64)
    h.assert_eq[None](None, (doc.data as JsonObject).data("c") as None)

    doc.parse("""{"a": true, "b": {"c": 52, "d": null}}""")
    h.assert_eq[USize](2, (doc.data as JsonObject).data.size())
    h.assert_eq[Bool](true, (doc.data as JsonObject).data("a") as Bool)
    h.assert_eq[USize](2,
      ((doc.data as JsonObject).data("b") as JsonObject).data.size())
    h.assert_eq[I64](52,
      ((doc.data as JsonObject).data("b") as JsonObject).data("c") as I64)
    h.assert_eq[None](None,
      ((doc.data as JsonObject).data("b") as JsonObject).data("d") as None)

    h.assert_error(lambda()? => JsonDoc.parse("""{"a": 1 "b": 2}""") end)
    h.assert_error(lambda()? => JsonDoc.parse("{,}") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a": true,}""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{,"a": true}""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a" """) end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a": """) end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a": true""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a": true,""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{"a" true}""") end)
    h.assert_error(lambda()? => JsonDoc.parse("""{:true}""") end)


class iso _TestParseRFC1 is UnitTest
  """
  Test Json parsing of first example from RFC7159.
  """
  fun name(): String => "JSON/parse.rfc1"

  fun apply(h: TestHelper) ? =>
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

    let doc: JsonDoc = JsonDoc
    doc.parse(src)

    let obj1 = doc.data as JsonObject

    h.assert_eq[USize](1, obj1.data.size())

    let obj2 = obj1.data("Image") as JsonObject

    h.assert_eq[USize](6, obj2.data.size())
    h.assert_eq[I64](800, obj2.data("Width") as I64)
    h.assert_eq[I64](600, obj2.data("Height") as I64)
    h.assert_eq[String]("View from 15th Floor", obj2.data("Title") as String)
    h.assert_eq[Bool](false, obj2.data("Animated") as Bool)

    let obj3 = obj2.data("Thumbnail") as JsonObject

    h.assert_eq[USize](3, obj3.data.size())
    h.assert_eq[I64](100, obj3.data("Width") as I64)
    h.assert_eq[I64](125, obj3.data("Height") as I64)
    h.assert_eq[String]("http://www.example.com/image/481989943",
      obj3.data("Url") as String)

    let array = obj2.data("IDs") as JsonArray

    h.assert_eq[USize](4, array.data.size())
    h.assert_eq[I64](116, array.data(0) as I64)
    h.assert_eq[I64](943, array.data(1) as I64)
    h.assert_eq[I64](234, array.data(2) as I64)
    h.assert_eq[I64](38793, array.data(3) as I64)


class iso _TestParseRFC2 is UnitTest
  """
  Test Json parsing of second example from RFC7159.
  """
  fun name(): String => "JSON/parse.rfc2"

  fun apply(h: TestHelper) ? =>
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

    let doc: JsonDoc = JsonDoc
    doc.parse(src)

    let array = doc.data as JsonArray

    h.assert_eq[USize](2, array.data.size())

    let obj1 = array.data(0) as JsonObject

    h.assert_eq[USize](8, obj1.data.size())
    h.assert_eq[String]("zip", obj1.data("precision") as String)
    h.assert_true(((obj1.data("Latitude") as F64) - 37.7668).abs() < 0.001)
    h.assert_true(((obj1.data("Longitude") as F64) + 122.3959).abs() < 0.001)
    h.assert_eq[String]("", obj1.data("Address") as String)
    h.assert_eq[String]("SAN FRANCISCO", obj1.data("City") as String)
    h.assert_eq[String]("CA", obj1.data("State") as String)
    h.assert_eq[String]("94107", obj1.data("Zip") as String)
    h.assert_eq[String]("US", obj1.data("Country") as String)

    let obj2 = array.data(1) as JsonObject

    h.assert_eq[USize](8, obj2.data.size())
    h.assert_eq[String]("zip", obj2.data("precision") as String)
    h.assert_true(((obj2.data("Latitude") as F64) - 37.371991).abs() < 0.001)
    h.assert_true(((obj2.data("Longitude") as F64) + 122.026020).abs() < 0.001)
    h.assert_eq[String]("", obj2.data("Address") as String)
    h.assert_eq[String]("SUNNYVALE", obj2.data("City") as String)
    h.assert_eq[String]("CA", obj2.data("State") as String)
    h.assert_eq[String]("94085", obj2.data("Zip") as String)
    h.assert_eq[String]("US", obj2.data("Country") as String)


class iso _TestPrintKeyword is UnitTest
  """
  Test Json printing of keywords.
  """
  fun name(): String => "JSON/print.keyword"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc

    doc.data = true
    h.assert_eq[String]("true", doc.string())

    doc.data = false
    h.assert_eq[String]("false", doc.string())

    doc.data = None
    h.assert_eq[String]("null", doc.string())


class iso _TestPrintNumber is UnitTest
  """
  Test Json printing of numbers.
  """
  fun name(): String => "JSON/print.number"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc

    doc.data = I64(0)
    h.assert_eq[String]("0", doc.string())

    doc.data = I64(13)
    h.assert_eq[String]("13", doc.string())

    doc.data = I64(-13)
    h.assert_eq[String]("-13", doc.string())

    doc.data = F64(0)
    h.assert_eq[String]("0.0", doc.string())

    doc.data = F64(1.5)
    h.assert_eq[String]("1.5", doc.string())

    doc.data = F64(-1.5)
    h.assert_eq[String]("-1.5", doc.string())

    // We don't test exponent formatted output because it can be slightly
    // different on different platforms


class iso _TestPrintString is UnitTest
  """
  Test Json printing of strings.
  """
  fun name(): String => "JSON/print.string"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc

    doc.data = "Foo"
    h.assert_eq[String](""""Foo"""", doc.string())

    doc.data = "Foo\tbar"
    h.assert_eq[String](""""Foo\tbar"""", doc.string())

    doc.data = "Foo\"bar"
    h.assert_eq[String](""""Foo\"bar"""", doc.string())

    doc.data = "Foo\\bar"
    h.assert_eq[String](""""Foo\\bar"""", doc.string())

    doc.data = "Foo\abar"
    h.assert_eq[String](""""Foo\u0007bar"""", doc.string())

    doc.data = "Foo\u1000bar"
    h.assert_eq[String](""""Foo\u1000bar"""", doc.string())

    doc.data = "Foo\U01D11Ebar"
    h.assert_eq[String](""""Foo\uD834\uDD1Ebar"""", doc.string())


class iso _TestPrintArray is UnitTest
  """
  Test Json printing of arrays.
  """
  fun name(): String => "JSON/print.array"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc
    let array: JsonArray = JsonArray

    doc.data = array
    h.assert_eq[String]("[]", doc.string())

    array.data.clear()
    array.data.push(true)
    h.assert_eq[String]("[\n  true\n]", doc.string())

    array.data.clear()
    array.data.push(true)
    array.data.push(false)
    h.assert_eq[String]("[\n  true,\n  false\n]", doc.string())

    array.data.clear()
    array.data.push(true)
    array.data.push(I64(13))
    array.data.push(None)
    h.assert_eq[String]("[\n  true,\n  13,\n  null\n]", doc.string())

    array.data.clear()
    array.data.push(true)
    var nested: JsonArray = JsonArray
    nested.data.push(I64(52))
    nested.data.push(None)
    array.data.push(nested)
    h.assert_eq[String]("[\n  true,\n  [\n    52,\n    null\n  ]\n]",
      doc.string())


class iso _TestNoPrettyPrintArray is UnitTest
  """
  Test Json none-pretty printing of arrays.
  """
  fun name(): String => "JSON/nopprint.array"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc
    let array: JsonArray = JsonArray

    doc.data = array
    h.assert_eq[String]("[]", doc.string(false))

    array.data.clear()
    array.data.push(true)
    h.assert_eq[String]("[true]", doc.string(false))

    array.data.clear()
    array.data.push(true)
    array.data.push(false)
    h.assert_eq[String]("[true, false]", doc.string(false))

    array.data.clear()
    array.data.push(true)
    array.data.push(I64(13))
    array.data.push(None)
    h.assert_eq[String]("[true, 13, null]", doc.string(false))

    array.data.clear()
    array.data.push(true)
    var nested: JsonArray = JsonArray
    nested.data.push(I64(52))
    nested.data.push(None)
    array.data.push(nested)
    h.assert_eq[String]("[true, [52, null]]",
      doc.string(false))

class iso _TestPrintObject is UnitTest
  """
  Test Json printing of objects.
  """
  fun name(): String => "JSON/print.object"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc
    let obj: JsonObject = JsonObject

    doc.data = obj
    h.assert_eq[String]("{}", doc.string())

    obj.data.clear()
    obj.data("foo") = true
    h.assert_eq[String]("{\n  \"foo\": true\n}", doc.string())

    obj.data.clear()
    obj.data("a") = true
    obj.data("b") = I64(3)
    let s = doc.string()
    h.assert_true((s == "{\n  \"a\": true,\n  \"b\": 3\n}") or
      (s == "{\n  \"b\": false,\n  \"a\": true\n}"))

    // We don't test with more fields in the object because we don't know what
    // order they will be printed in

class iso _TestNoPrettyPrintObject is UnitTest
  """
  Test Json none-pretty printing of objects.
  """
  fun name(): String => "JSON/nopprint.object"

  fun apply(h: TestHelper) =>
    let doc: JsonDoc = JsonDoc
    let obj: JsonObject = JsonObject

    doc.data = obj
    h.assert_eq[String]("{}", doc.string(false))

    obj.data.clear()
    obj.data("foo") = true
    h.assert_eq[String]("{\"foo\": true}", doc.string(false))

    obj.data.clear()
    obj.data("a") = true
    obj.data("b") = I64(3)
    let s = doc.string(false)
    h.assert_true((s == "{\"a\": true, \"b\": 3}") or
      (s == "{\"b\": 3, \"a\": true}"))

    // We don't test with more fields in the object because we don't know what
    // order they will be printed in

class iso _TestParsePrint is UnitTest
  """
  Test Json parsing a complex example and then reprinting it.
  """
  fun name(): String => "JSON/parseprint"

  fun apply(h: TestHelper) ? =>
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

    let doc: JsonDoc = JsonDoc
    doc.parse(src)
    let printed = doc.string()

    // TODO: Sort out line endings on different platforms. For now normalise
    // before comparing
    let actual: String ref = printed.clone()
    actual.remove("\r")

    let expect: String ref = src.clone()
    expect.remove("\r")

    h.assert_eq[String ref](expect, actual)
