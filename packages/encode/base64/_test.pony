use "pony_test"
use "collections"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestBase64Decode)
    test(_TestBase64Encode)
    test(_TestBase64EncodeDecode)
    test(_TestBase64EncodeURL)
    test(_TestBase64EncodeURLDecodeURL)
    test(_TestBase64Quote)

class \nodoc\ iso _TestBase64Encode is UnitTest
  """
  Test base64 encoding.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.encode"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("", Base64.encode(""))
    h.assert_eq[String]("Zg==", Base64.encode("f"))
    h.assert_eq[String]("Zm8=", Base64.encode("fo"))
    h.assert_eq[String]("Zm9v", Base64.encode("foo"))
    h.assert_eq[String]("Zm9vYg==", Base64.encode("foob"))
    h.assert_eq[String]("Zm9vYmE=", Base64.encode("fooba"))
    h.assert_eq[String]("Zm9vYmFy", Base64.encode("foobar"))

    // pad = None omits the trailing padding rather than emitting it.
    h.assert_eq[String]("", Base64.encode("" where pad = None))
    h.assert_eq[String]("Zg", Base64.encode("f" where pad = None))
    h.assert_eq[String]("Zm8", Base64.encode("fo" where pad = None))
    h.assert_eq[String]("Zm9v", Base64.encode("foo" where pad = None))
    h.assert_eq[String]("Zm9vYg", Base64.encode("foob" where pad = None))
    h.assert_eq[String]("Zm9vYmE", Base64.encode("fooba" where pad = None))
    h.assert_eq[String]("Zm9vYmFy", Base64.encode("foobar" where pad = None))

class \nodoc\ iso _TestBase64Decode is UnitTest
  """
  Test base64 decoding. Examples with and without padding are tested.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode/Base64.decode"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[String]("", Base64.decode[String iso]("")?)
    h.assert_eq[String]("f", Base64.decode[String iso]("Zg==")?)
    h.assert_eq[String]("fo", Base64.decode[String iso]("Zm8=")?)
    h.assert_eq[String]("foo", Base64.decode[String iso]("Zm9v")?)
    h.assert_eq[String]("foob", Base64.decode[String iso]("Zm9vYg==")?)
    h.assert_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE=")?)
    h.assert_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy")?)

    h.assert_eq[String]("", Base64.decode[String iso]("")?)
    h.assert_eq[String]("f", Base64.decode[String iso]("Zg")?)
    h.assert_eq[String]("fo", Base64.decode[String iso]("Zm8")?)
    h.assert_eq[String]("foo", Base64.decode[String iso]("Zm9v")?)
    h.assert_eq[String]("foob", Base64.decode[String iso]("Zm9vYg")?)
    h.assert_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE")?)
    h.assert_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy")?)

class \nodoc\ iso _TestBase64EncodeURL is UnitTest
  """
  Test base64 URL-safe encoding (RFC 4648 section 5), with and without padding.
  """
  fun name(): String => "encode/Base64.encode_url"

  fun apply(h: TestHelper) =>
    // Padding is omitted by default: the trailing characters are not emitted
    // at all, rather than replaced by NUL bytes.
    h.assert_eq[String]("", Base64.encode_url(""))
    h.assert_eq[String]("Zg", Base64.encode_url("f"))
    h.assert_eq[String]("Zm8", Base64.encode_url("fo"))
    h.assert_eq[String]("Zm9v", Base64.encode_url("foo"))
    h.assert_eq[String]("Zm9vYg", Base64.encode_url("foob"))
    h.assert_eq[String]("Zm9vYmE", Base64.encode_url("fooba"))
    h.assert_eq[String]("Zm9vYmFy", Base64.encode_url("foobar"))

    // With pad = true, the trailing '=' padding is emitted as for the standard
    // alphabet.
    h.assert_eq[String]("", Base64.encode_url("", true))
    h.assert_eq[String]("Zg==", Base64.encode_url("f", true))
    h.assert_eq[String]("Zm8=", Base64.encode_url("fo", true))
    h.assert_eq[String]("Zm9v", Base64.encode_url("foo", true))
    h.assert_eq[String]("Zm9vYg==", Base64.encode_url("foob", true))
    h.assert_eq[String]("Zm9vYmE=", Base64.encode_url("fooba", true))
    h.assert_eq[String]("Zm9vYmFy", Base64.encode_url("foobar", true))

    // The URL-safe alphabet uses '-' for index 62 and '_' for index 63, where
    // the standard alphabet uses '+' and '/'.
    let url_chars = recover val [as U8: 0xFB; 0xF0] end
    h.assert_eq[String]("-_A", Base64.encode_url(url_chars))
    h.assert_eq[String]("-_A=", Base64.encode_url(url_chars, true))

class \nodoc\ iso _TestBase64EncodeURLDecodeURL is UnitTest
  """
  encode_url then decode_url round-trips the original bytes, with and without
  padding, across inputs whose length mod 3 hits every remainder (0, 1, 2).
  """
  fun name(): String => "encode/Base64.encodeurldecodeurl"

  fun apply(h: TestHelper) ? =>
    let inputs =
      [ ""; "f"; "fo"; "foo"; "foob"; "fooba"; "foobar"; "any carnal pleasure." ]

    for src in inputs.values() do
      for pad in [false; true].values() do
        let enc = recover val Base64.encode_url[String iso](src, pad) end
        let dec = recover val Base64.decode_url[String iso](enc)? end
        h.assert_eq[String](src, dec)
      end
    end

class \nodoc\ iso _TestBase64EncodeDecode is UnitTest
  """
  Test base64 encoding.
  Check encoding then decoding gives back original.
  """
  fun name(): String => "encode/Base64.encodedecode"

  fun apply(h: TestHelper) ? =>
    let src = "Check encoding then decoding gives back original."
    let enc = recover val Base64.encode(src) end
    let dec = recover val Base64.decode[String iso](enc)? end

    h.assert_eq[String](src, dec)

class \nodoc\ iso _TestBase64Quote is UnitTest
  """
  Test base64 encoding.
  Check encoding then decoding something a bit longer.
  """
  fun name(): String => "encode/Base64.quote"

  fun apply(h: TestHelper) ? =>
    let src =
      recover val
        "Man is distinguished, not only by his reason, but by this singular " +
        "passion from other animals, which is a lust of the mind, that by a " +
        "perseverance of delight in the continued and indefatigable " +
        "generation of knowledge, exceeds the short vehemence of any carnal " +
        "pleasure."
      end

    let expect =
      recover val
        "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBie" +
        "SB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcy" +
        "BhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWd" +
        "odCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yg" +
        "a25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hb" +
        "CBwbGVhc3VyZS4="
      end

    let enc = recover val Base64.encode(src) end
    h.assert_eq[String](expect, enc)

    let dec = recover val Base64.decode[String iso](enc)? end
    h.assert_eq[String](src, dec)
