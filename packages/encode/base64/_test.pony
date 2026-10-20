use "pony_test"
use "collections"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestBase64Decode)
    test(_TestBase64Encode)
    test(_TestBase64EncodeDecodeProperty)
    test(_TestBase64EncodeDecode)
    test(_TestBase64Quote)
    test(_TestBase64URLAlphabetOracleProperty)
    test(_TestBase64URLDecode)
    test(_TestBase64URLEncode)
    test(_TestBase64URLPaddingProperty)
    test(_TestBase64URLRoundtrip)
    test(_TestBase64URLRoundtripProperty)
    test(_TestBase64URLUnpaddedProperty)

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

class \nodoc\ iso _TestBase64URLEncode is UnitTest
  """
  Test URL-safe base64 encoding (RFC 4648 section 5), with and without
  padding.
  """
  fun name(): String => "encode/Base64.encode_url"

  fun apply(h: TestHelper) =>
    // RFC 4648 test vectors, unpadded
    h.assert_eq[String]("", Base64.encode_url(""))
    h.assert_eq[String]("Zg", Base64.encode_url("f"))
    h.assert_eq[String]("Zm8", Base64.encode_url("fo"))
    h.assert_eq[String]("Zm9v", Base64.encode_url("foo"))
    h.assert_eq[String]("Zm9vYg", Base64.encode_url("foob"))
    h.assert_eq[String]("Zm9vYmE", Base64.encode_url("fooba"))
    h.assert_eq[String]("Zm9vYmFy", Base64.encode_url("foobar"))

    // With padding
    h.assert_eq[String]("Zg==", Base64.encode_url("f" where pad = true))
    h.assert_eq[String]("Zm8=", Base64.encode_url("fo" where pad = true))

    // URL alphabet: '-' at index 62, '_' at index 63
    h.assert_eq[String](
      "----", Base64.encode_url([as U8: 0xFB; 0xEF; 0xBE]))
    h.assert_eq[String](
      "____", Base64.encode_url([as U8: 0xFF; 0xFF; 0xFF]))

class \nodoc\ iso _TestBase64URLDecode is UnitTest
  """
  Test URL-safe base64 decoding (RFC 4648 section 5).
  Accepts both padded and unpadded input.
  """
  fun name(): String => "encode/Base64.decode_url"

  fun apply(h: TestHelper) ? =>
    // Without padding
    h.assert_eq[String]("", Base64.decode_url[String iso]("")?)
    h.assert_eq[String]("f", Base64.decode_url[String iso]("Zg")?)
    h.assert_eq[String]("fo", Base64.decode_url[String iso]("Zm8")?)
    h.assert_eq[String]("foo", Base64.decode_url[String iso]("Zm9v")?)
    h.assert_eq[String]("foob", Base64.decode_url[String iso]("Zm9vYg")?)
    h.assert_eq[String]("fooba", Base64.decode_url[String iso]("Zm9vYmE")?)
    h.assert_eq[String]("foobar", Base64.decode_url[String iso]("Zm9vYmFy")?)

    // With padding
    h.assert_eq[String]("f", Base64.decode_url[String iso]("Zg==")?)
    h.assert_eq[String]("fo", Base64.decode_url[String iso]("Zm8=")?)

    // URL alphabet
    h.assert_array_eq[U8](
      [as U8: 0xFB; 0xEF; 0xBE],
      Base64.decode_url[Array[U8] iso]("----")?)
    h.assert_array_eq[U8](
      [as U8: 0xFF; 0xFF; 0xFF],
      Base64.decode_url[Array[U8] iso]("____")?)

class \nodoc\ iso _TestBase64URLRoundtrip is UnitTest
  """
  Encoding then decoding with the URL-safe variant recovers the original.
  """
  fun name(): String => "encode/Base64.url_roundtrip"

  fun apply(h: TestHelper) ? =>
    let src = "Check encoding then decoding gives back original."
    let enc = recover val Base64.encode_url(src) end
    let dec = recover val Base64.decode_url[String iso](enc)? end
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
        "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb" +
        "24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlci" +
        "BhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQ" +
        "gYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu" +
        "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ" +
        "GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm" +
        "5hbCBwbGVhc3VyZS4="
      end

    let enc = recover val Base64.encode(src) end
    h.assert_eq[String](expect, enc)

    let dec = recover val Base64.decode[String iso](enc)? end
    h.assert_eq[String](src, dec)

class \nodoc\ iso _TestBase64EncodeDecodeProperty is UnitTest
  """
  Decoding the standard encoding of any byte sequence returns the original.
  """
  fun name(): String => "encode/Base64.property/encode_decode"

  fun apply(h: TestHelper) ? =>
    h.for_all[Array[U8]](
      recover val _B64.bytes() end)(
      {(sample, ph) ? =>
        let enc: String val = Base64.encode(sample)
        let dec: Array[U8] val = Base64.decode[Array[U8] iso](enc)?
        ph.assert_array_eq[U8](sample, dec)
      })?

class \nodoc\ iso _TestBase64URLRoundtripProperty is UnitTest
  """
  encode_url then decode_url returns the original bytes, with and without
  padding.
  """
  fun name(): String => "encode/Base64.property/url_roundtrip"

  fun apply(h: TestHelper) ? =>
    h.for_all2[Array[U8], Bool](
      recover val _B64.bytes() end,
      recover val Generators.bool() end)(
      {(sample, pad, ph) ? =>
        let enc: String val =
          Base64.encode_url[String iso](sample, pad)
        let dec: Array[U8] val =
          Base64.decode_url[Array[U8] iso](enc)?
        ph.assert_array_eq[U8](sample, dec)
      })?

class \nodoc\ iso _TestBase64URLUnpaddedProperty is UnitTest
  """
  Unpadded URL encoding has the correct length and contains only URL-safe
  characters. No NUL bytes, no '='.
  """
  fun name(): String => "encode/Base64.property/url_unpadded"

  fun apply(h: TestHelper) ? =>
    h.for_all[Array[U8]](
      recover val _B64.bytes() end)(
      {(sample, ph) =>
        let enc: String val =
          Base64.encode_url[String iso](sample, false)
        let n = sample.size()
        let rem =
          match n % 3
          | 1 => USize(2)
          | 2 => USize(3)
          else USize(0)
          end
        ph.assert_eq[USize](
          ((n / 3) * 4) + rem,
          enc.size(),
          "unpadded url output has the wrong length")
        for b in enc.values() do
          if not _B64.is_url_char(b) then
            ph.fail(
              "unpadded url output contains a non-url-safe byte: " +
                b.string())
            break
          end
        end
      })?

class \nodoc\ iso _TestBase64URLPaddingProperty is UnitTest
  """
  The unpadded URL encoding equals the padded one with trailing '=' stripped.
  """
  fun name(): String => "encode/Base64.property/url_padding"

  fun apply(h: TestHelper) ? =>
    h.for_all[Array[U8]](
      recover val _B64.bytes() end)(
      {(sample, ph) =>
        let padded: String val =
          Base64.encode_url[String iso](sample, true)
        let unpadded: String val =
          Base64.encode_url[String iso](sample, false)
        let stripped: String val =
          recover val
            let s = String(padded.size())
            for b in padded.values() do
              if b != '=' then s.push(b) end
            end
            s
          end
        ph.assert_eq[String](unpadded, stripped)
      })?

class \nodoc\ iso _TestBase64URLAlphabetOracleProperty is UnitTest
  """
  The padded URL encoding equals the standard encoding with '+' replaced by
  '-' and '/' replaced by '_'.
  """
  fun name(): String => "encode/Base64.property/url_alphabet_oracle"

  fun apply(h: TestHelper) ? =>
    h.for_all[Array[U8]](
      recover val _B64.bytes() end)(
      {(sample, ph) =>
        let std: String val = Base64.encode(sample)
        let url: String val =
          Base64.encode_url[String iso](sample, true)
        let translated: String val =
          recover val
            let s = String(std.size())
            for b in std.values() do
              s.push(
                match b
                | '+' => '-'
                | '/' => '_'
                else b
                end)
            end
            s
          end
        ph.assert_eq[String](translated, url)
      })?

primitive \nodoc\ _B64
  fun bytes(
    min: USize = 0,
    max: USize = 64)
    : Generator[Array[U8]]
  =>
    Generators.array_of[U8](
      Generators.frequency[U8](
        [ as WeightedGenerator[U8]:
          (6, Generators.u8())
          (1, Generators.one_of[U8]([as U8: 0; 1; 62; 63; 254; 255]))
        ]),
      min,
      max)

  fun is_url_char(b: U8): Bool =>
    ((b >= 'A') and (b <= 'Z')) or
    ((b >= 'a') and (b <= 'z')) or
    ((b >= '0') and (b <= '9')) or
    (b == '-') or (b == '_')
