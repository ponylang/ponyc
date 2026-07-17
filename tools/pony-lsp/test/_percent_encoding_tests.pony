use "pony_check"
use "pony_test"
use ".."

primitive \nodoc\ _PercentEncodingTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_PercentEncodingEncodeTest)
    test(_PercentEncodingEncodeLiteralTest)
    test(_PercentEncodingEncodeBoundaryTest)
    test(_PercentEncodingEncodeHexCaseTest)
    test(_PercentEncodingDecodeTest)
    test(_PercentEncodingDecodeHexCaseTest)
    test(_PercentEncodingDecodeMalformedTest)
    test(_PercentEncodingRoundTripTest)
    test(_PercentEncodingRoundTripPropertyTest)

class \nodoc\ iso _PercentEncodingEncodeTest is UnitTest
  fun name(): String => "percent_encoding/encode"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("a%20b", PercentEncoding.encode("a b"))
    // A literal percent must encode, or a decoding reader would read the
    // bytes after it as an escape.
    h.assert_eq[String]("100%25", PercentEncoding.encode("100%"))
    // '#' and '?' would otherwise start a fragment or a query.
    h.assert_eq[String]("c%231", PercentEncoding.encode("c#1"))
    h.assert_eq[String]("c%3F1", PercentEncoding.encode("c?1"))
    h.assert_eq[String]("%5Bx%5D", PercentEncoding.encode("[x]"))
    // One escape per byte of a multi-byte UTF-8 sequence.
    h.assert_eq[String]("caf%C3%A9", PercentEncoding.encode("café"))
    h.assert_eq[String]("", PercentEncoding.encode(""))

class \nodoc\ iso _PercentEncodingEncodeLiteralTest is UnitTest
  """
  RFC 3986 pchar, plus the '/' separator, survives encoding unchanged.
  """
  fun name(): String => "percent_encoding/encode/literal"

  fun apply(h: TestHelper) =>
    let unreserved = "azAZ09-._~"
    h.assert_eq[String](unreserved, PercentEncoding.encode(unreserved))
    let sub_delims = "!$&'()*+,;="
    h.assert_eq[String](sub_delims, PercentEncoding.encode(sub_delims))
    h.assert_eq[String](":@", PercentEncoding.encode(":@"))
    h.assert_eq[String]("/a/b/c", PercentEncoding.encode("/a/b/c"))

class \nodoc\ iso _PercentEncodingEncodeBoundaryTest is UnitTest
  """
  The bytes just outside each literal range encode.

  A round trip cannot pin these: drop a byte from the literal set and encode
  emits an escape that decode turns straight back, so the pair still agrees.
  """
  fun name(): String => "percent_encoding/encode/boundary"

  fun apply(h: TestHelper) =>
    // '`' is 'a' - 1 and '{' is 'z' + 1
    h.assert_eq[String]("%60%7B", PercentEncoding.encode("`{"))
    // '@' is 'A' - 1 and '[' is 'Z' + 1; '@' is pchar, '[' is not
    h.assert_eq[String]("@%5B", PercentEncoding.encode("@["))
    // '/' is '0' - 1 and ':' is '9' + 1; both are pchar
    h.assert_eq[String]("/:", PercentEncoding.encode("/:"))

class \nodoc\ iso _PercentEncodingEncodeHexCaseTest is UnitTest
  """
  RFC 3986 section 2.1 specifies uppercase hex digits for URI producers.
  """
  fun name(): String => "percent_encoding/encode/hex_case"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("%5B", PercentEncoding.encode("["))
    h.assert_eq[String]("%7C", PercentEncoding.encode("|"))
    h.assert_eq[String]("%C3%A9", PercentEncoding.encode("é"))

class \nodoc\ iso _PercentEncodingDecodeTest is UnitTest
  fun name(): String => "percent_encoding/decode"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("a b", PercentEncoding.decode("a%20b"))
    h.assert_eq[String]("100%", PercentEncoding.decode("100%25"))
    h.assert_eq[String]("c#1", PercentEncoding.decode("c%231"))
    h.assert_eq[String]("café", PercentEncoding.decode("caf%C3%A9"))
    h.assert_eq[String]("/a/b", PercentEncoding.decode("%2Fa%2Fb"))
    h.assert_eq[String]("", PercentEncoding.decode(""))
    h.assert_eq[String]("nothing", PercentEncoding.decode("nothing"))

class \nodoc\ iso _PercentEncodingDecodeHexCaseTest is UnitTest
  """
  Neovim emits lowercase hex digits, so both cases have to decode.
  """
  fun name(): String => "percent_encoding/decode/hex_case"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("café", PercentEncoding.decode("caf%c3%a9"))
    h.assert_eq[String]("[x]", PercentEncoding.decode("%5bx%5d"))
    h.assert_eq[String]("<>", PercentEncoding.decode("%3c%3E"))
    // 'f' is the top of the lowercase range, and 'e' its neighbour.
    h.assert_eq[String]("ÿ", PercentEncoding.decode("%c3%bf"))
    h.assert_eq[String]("î", PercentEncoding.decode("%c3%ae"))

class \nodoc\ iso _PercentEncodingDecodeMalformedTest is UnitTest
  """
  A '%' that does not introduce two hex digits stands for itself.
  """
  fun name(): String => "percent_encoding/decode/malformed"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("%ZZ", PercentEncoding.decode("%ZZ"))
    h.assert_eq[String]("%", PercentEncoding.decode("%"))
    h.assert_eq[String]("a%", PercentEncoding.decode("a%"))
    h.assert_eq[String]("%A", PercentEncoding.decode("%A"))
    h.assert_eq[String]("%2", PercentEncoding.decode("%2"))
    h.assert_eq[String]("%G0", PercentEncoding.decode("%G0"))
    // The first '%' is literal; the second introduces a real escape.
    h.assert_eq[String]("% ", PercentEncoding.decode("%%20"))

class \nodoc\ iso _PercentEncodingRoundTripTest is UnitTest
  fun name(): String => "percent_encoding/round_trip"

  fun apply(h: TestHelper) =>
    let cases =
      [ "a b"; "100%"; "50%20off"; "café"; "c#1"; "/a/b"; ""
        "!$&'()*+,;=:@"; "%%%"; "a\nb" ]
    for s in cases.values() do
      h.assert_eq[String](
        s,
        PercentEncoding.decode(PercentEncoding.encode(s)),
        "round trip of " + s)
    end

class \nodoc\ iso _PercentEncodingRoundTripPropertyTest is UnitTest
  """
  decode undoes encode for any string of bytes.

  The codec takes bytes rather than a path, so this needs no constraints on
  what it generates. It reaches the high half of the byte range, which the
  worked examples reach only through the two bytes of "é".
  """
  fun name(): String => "percent_encoding/round_trip/property"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[String](
      recover val
        Generators.byte_string(Generators.u8(1, 255), 0, 30)
      end,
      h)(
      {(s: String, ph: PropertyHelper) =>
        ph.assert_eq[String](
          s,
          PercentEncoding.decode(PercentEncoding.encode(s)))
      })?
