use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBase64Encode)
    test(_TestBase64Decode)
    test(_TestBase64EncodeDecode)
    test(_TestBase64Quote)

class iso _TestBase64Encode is UnitTest
  """
  Test base64 encoding.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.encode"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("", String.from_iso_array(Base64.encode("".array())))
    h.assert_eq[String]("Zg==", String.from_iso_array(Base64.encode("f".array())))
    h.assert_eq[String]("Zm8=", String.from_iso_array(Base64.encode("fo".array())))
    h.assert_eq[String]("Zm9v", String.from_iso_array(Base64.encode("foo".array())))
    h.assert_eq[String]("Zm9vYg==", String.from_iso_array(Base64.encode("foob".array())))
    h.assert_eq[String]("Zm9vYmE=", String.from_iso_array(Base64.encode("fooba".array())))
    h.assert_eq[String]("Zm9vYmFy", String.from_iso_array(Base64.encode("foobar".array())))

class iso _TestBase64Decode is UnitTest
  """
  Test base64 decoding. Examples with and without padding are tested.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.decode"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[String]("", String.from_iso_array(Base64.decode("".array())?))
    h.assert_eq[String]("f", String.from_iso_array(Base64.decode("Zg==".array())?))
    h.assert_eq[String]("fo", String.from_iso_array(Base64.decode("Zm8=".array())?))
    h.assert_eq[String]("foo", String.from_iso_array(Base64.decode("Zm9v".array())?))
    h.assert_eq[String]("foob", String.from_iso_array(Base64.decode("Zm9vYg==".array())?))
    h.assert_eq[String]("fooba", String.from_iso_array(Base64.decode("Zm9vYmE=".array())?))
    h.assert_eq[String]("foobar", String.from_iso_array(Base64.decode("Zm9vYmFy".array())?))

    h.assert_eq[String]("", String.from_iso_array(Base64.decode("".array())?))
    h.assert_eq[String]("f", String.from_iso_array(Base64.decode("Zg".array())?))
    h.assert_eq[String]("fo", String.from_iso_array(Base64.decode("Zm8".array())?))
    h.assert_eq[String]("foo", String.from_iso_array(Base64.decode("Zm9v".array())?))
    h.assert_eq[String]("foob", String.from_iso_array(Base64.decode("Zm9vYg".array())?))
    h.assert_eq[String]("fooba", String.from_iso_array(Base64.decode("Zm9vYmE".array())?))
    h.assert_eq[String]("foobar", String.from_iso_array(Base64.decode("Zm9vYmFy".array())?))

class iso _TestBase64EncodeDecode is UnitTest
  """
  Test base64 encoding.
  Check encoding then decoding gives back original.
  """
  fun name(): String => "encode/Base64.encodedecode"

  fun apply(h: TestHelper) ? =>
    let src = "Check encoding then decoding gives back original."
    let enc = recover val Base64.encode(src.array()) end
    let dec = recover val Base64.decode(enc)? end

    h.assert_eq[String](src, String.from_array(dec))

class iso _TestBase64Quote is UnitTest
  """
  Test base64 encoding.
  Check encoding then decoding something a bit longer.
  """
  fun name(): String => "encode/Base64.quote"

  fun apply(h: TestHelper) ? =>
    let src =
      "Man is distinguished, not only by his reason, but by this singular " +
      "passion from other animals, which is a lust of the mind, that by a " +
      "perseverance of delight in the continued and indefatigable " +
      "generation of knowledge, exceeds the short vehemence of any carnal " +
      "pleasure."

    let expect =
      "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBie" +
      "SB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcy" +
      "BhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWd" +
      "odCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yg" +
      "a25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hb" +
      "CBwbGVhc3VyZS4="

    let enc = recover val String.from_iso_array(Base64.encode(src.array())) end
    h.assert_eq[String](expect, enc)

    let dec = recover val String.from_iso_array(Base64.decode(enc.array())?) end
    h.assert_eq[String](src, dec)
