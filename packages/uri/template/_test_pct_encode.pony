use "pony_test"
use "pony_check"

class \nodoc\ iso _TestPctEncodeUnreservedPassthrough is UnitTest
  """Unreserved characters are never pct-encoded in either mode."""
  fun name(): String => "uri/template/pct_encode/unreserved passthrough"

  fun apply(h: TestHelper) =>
    let unreserved =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
    let result_unreserved = _PctEncode.encode(unreserved, false)
    h.assert_eq[String val](unreserved, consume result_unreserved)

    let result_reserved = _PctEncode.encode(unreserved, true)
    h.assert_eq[String val](unreserved, consume result_reserved)

class \nodoc\ iso _TestPctEncodeSpecialChars is UnitTest
  """Special characters are pct-encoded in unreserved mode."""
  fun name(): String => "uri/template/pct_encode/special chars encoded"

  fun apply(h: TestHelper) =>
    h.assert_eq[String val]("%20", _PctEncode.encode(" ", false))
    h.assert_eq[String val]("%21", _PctEncode.encode("!", false))
    h.assert_eq[String val]("%2F", _PctEncode.encode("/", false))
    h.assert_eq[String val]("%3A", _PctEncode.encode(":", false))
    h.assert_eq[String val]("%23", _PctEncode.encode("#", false))
    h.assert_eq[String val]("%5B", _PctEncode.encode("[", false))
    h.assert_eq[String val]("%5D", _PctEncode.encode("]", false))

class \nodoc\ iso _TestPctEncodeReservedPassthrough is UnitTest
  """Reserved characters pass through in reserved mode."""
  fun name(): String => "uri/template/pct_encode/reserved passthrough"

  fun apply(h: TestHelper) =>
    let reserved = ":/?#[]@!$&'()*+,;="
    let result = _PctEncode.encode(reserved, true)
    h.assert_eq[String val](reserved, consume result)

class \nodoc\ iso _TestPctEncodeMultibyteUtf8 is UnitTest
  """Multi-byte UTF-8 characters are pct-encoded byte by byte."""
  fun name(): String => "uri/template/pct_encode/multibyte UTF-8"

  fun apply(h: TestHelper) =>
    // é is U+00E9, encoded as C3 A9 in UTF-8
    let e_acute =
      recover val
        String(2)
          .> push(0xC3)
          .> push(0xA9)
      end
    h.assert_eq[String val]("%C3%A9", _PctEncode.encode(e_acute, false))

    // € is U+20AC, encoded as E2 82 AC in UTF-8
    let euro =
      recover val
        String(3)
          .> push(0xE2)
          .> push(0x82)
          .> push(0xAC)
      end
    h.assert_eq[String val](
      "%E2%82%AC", _PctEncode.encode(euro, false))

class \nodoc\ iso _TestPctEncodeExistingTriplets is UnitTest
  """
  Existing %XX triplets pass through in reserved mode, re-encoded
  otherwise.
  """
  fun name(): String => "uri/template/pct_encode/existing triplets"

  fun apply(h: TestHelper) =>
    // Reserved mode: pass through existing triplets
    h.assert_eq[String val]("%20", _PctEncode.encode("%20", true))
    h.assert_eq[String val]("%2F", _PctEncode.encode("%2F", true))

    // Unreserved mode: re-encode the % sign
    h.assert_eq[String val]("%2520", _PctEncode.encode("%20", false))

class \nodoc\ iso _TestPctEncodeMixedContent is UnitTest
  """Mixed content with unreserved, reserved, and multi-byte chars."""
  fun name(): String => "uri/template/pct_encode/mixed content"

  fun apply(h: TestHelper) =>
    // Space is encoded in both modes (not unreserved, not reserved)
    h.assert_eq[String val](
      "hello%20world", _PctEncode.encode("hello world", false))
    h.assert_eq[String val](
      "hello%20world", _PctEncode.encode("hello world", true))

class \nodoc\ iso _TestPctEncodePropertyUnreserved is Property1[String]
  """Property: unreserved-only strings pass through unchanged."""
  fun name(): String =>
    "uri/template/pct_encode/property: unreserved passthrough"

  fun gen(): Generator[String] =>
    // Generate strings from unreserved character set only
    let unreserved_bytes: Array[U8] val =
      recover val
        let arr = Array[U8]
        let chars =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
        for ch in chars.values() do
          arr.push(ch)
        end
        arr
      end
    Generators.usize(0, unreserved_bytes.size() - 1)
      .map[U8]({(idx) =>
        try unreserved_bytes(idx)? else 'a' end
      })
      .flat_map[String]({(byte) =>
        Generators.byte_string(
          Generators.usize(0, unreserved_bytes.size() - 1)
            .map[U8]({(idx) =>
              try unreserved_bytes(idx)? else 'a' end
            }),
          0,
          50)
      })

  fun ref property(arg1: String, h: PropertyHelper) =>
    let result: String val = _PctEncode.encode(arg1, false)
    h.assert_eq[String val](arg1, result)

class \nodoc\ iso _TestPctEncodePropertyRoundtrip is Property1[String]
  """Property: decoding an unreserved-mode encoding yields the original."""
  fun name(): String => "uri/template/pct_encode/property: roundtrip"

  fun gen(): Generator[String] =>
    Generators.ascii_printable(0, 50)

  fun ref property(arg1: String, h: PropertyHelper) =>
    let encoded: String val = _PctEncode.encode(arg1, false)
    let decoded = _pct_decode(encoded)
    h.assert_eq[String val](arg1, decoded)

  fun _pct_decode(s: String): String val =>
    let out = recover iso String(s.size()) end
    var i: USize = 0
    while i < s.size() do
      try
        if (s(i)? == '%') and ((i + 2) < s.size()) then
          let hi = _unhex(s(i + 1)?)
          let lo = _unhex(s(i + 2)?)
          out.push((hi << 4) or lo)
          i = i + 3
        else
          out.push(s(i)?)
          i = i + 1
        end
      else
        break
      end
    end
    consume out

  fun _unhex(byte: U8): U8 =>
    if (byte >= '0') and (byte <= '9') then
      byte - '0'
    elseif (byte >= 'A') and (byte <= 'F') then
      (byte - 'A') + 10
    elseif (byte >= 'a') and (byte <= 'f') then
      (byte - 'a') + 10
    else
      0
    end

class \nodoc\ iso _TestPctEncodePropertyReservedSuperset is Property1[String]
  """
  Property: reserved encoding passes through everything unreserved does,
  plus more.
  """
  fun name(): String => "uri/template/pct_encode/property: reserved is superset"

  fun gen(): Generator[String] =>
    Generators.ascii_printable(1, 50)

  fun ref property(arg1: String, h: PropertyHelper) =>
    let unreserved_result: String val = _PctEncode.encode(arg1, false)
    let reserved_result: String val = _PctEncode.encode(arg1, true)
    // Reserved encoding should be same length or shorter (fewer things encoded)
    h.assert_true(reserved_result.size() <= unreserved_result.size())
