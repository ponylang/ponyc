use "pony_test"
use "pony_check"
use "collections"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestBase64Decode)
    test(_TestBase64Encode)
    test(_TestBase64EncodeAlphabetProperty)
    test(_TestBase64EncodeDecode)
    test(_TestBase64EncodeDecodeProperty)
    test(_TestBase64EncodePaddedLengthProperty)
    test(_TestBase64EncodeURL)
    test(_TestBase64EncodeURLDecodeURL)
    test(_TestBase64EncodeURLPaddingProperty)
    test(_TestBase64EncodeURLRoundtripProperty)
    test(_TestBase64EncodeURLUnpaddedProperty)
    test(_TestBase64Quote)
    test(_TestBase64URLAlphabetOracleProperty)
    test(_TestBase64WrappedLineLengthProperty)
    test(_TestBase64WrappedRoundtripProperty)

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

// Property-based tests. Each exercises one of the Base64 functions over
// generated byte sequences rather than a handful of fixed vectors. The
// generator (_B64.bytes) mixes uniform bytes with the boundary values that
// matter here — in particular NUL, the byte the old encode_url emitted where
// padding belonged.

class \nodoc\ iso _TestBase64EncodeDecodeProperty is UnitTest
  """
  Round-trip: decoding the standard RFC 4648 encoding of any byte sequence
  returns the original bytes.
  """
  fun name(): String => "encode/Base64.property/encode_decode"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) ? =>
        let enc: String val = Base64.encode(sample)
        let dec: Array[U8] val = Base64.decode[Array[U8] iso](enc)?
        ph.assert_array_eq[U8](sample, dec)
      })?

class \nodoc\ iso _TestBase64EncodeURLRoundtripProperty is UnitTest
  """
  Round-trip: encode_url then decode_url returns the original bytes, with and
  without padding.
  """
  fun name(): String => "encode/Base64.property/encode_url_decode_url"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all2[Array[U8], Bool](
      _B64.bytes(), recover val Generators.bool() end, h)(
      {(sample, pad, ph) ? =>
        let enc: String val = Base64.encode_url[String iso](sample, pad)
        let dec: Array[U8] val = Base64.decode_url[Array[U8] iso](enc)?
        ph.assert_array_eq[U8](sample, dec)
      })?

class \nodoc\ iso _TestBase64WrappedRoundtripProperty is UnitTest
  """
  Round-trip through the line-wrapped PEM and MIME encodings. decode skips the
  CR/LF line separators, so both wrapped forms must recover the original bytes.
  Inputs run up to 256 bytes so the 64/76-column wrapping actually fires.
  """
  fun name(): String => "encode/Base64.property/wrapped_roundtrip"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(0, 256), h)(
      {(sample, ph) ? =>
        let pem: String val = Base64.encode_pem(sample)
        let mime: String val = Base64.encode_mime(sample)
        ph.assert_array_eq[U8](sample, Base64.decode[Array[U8] iso](pem)?)
        ph.assert_array_eq[U8](sample, Base64.decode[Array[U8] iso](mime)?)
      })?

class \nodoc\ iso _TestBase64EncodePaddedLengthProperty is UnitTest
  """
  The standard encoding is always padded to a whole number of 4-character
  quanta, so its length is a multiple of 4.
  """
  fun name(): String => "encode/Base64.property/encode_padded_length"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) =>
        let enc: String val = Base64.encode(sample)
        ph.assert_eq[USize](0, enc.size() % 4,
          "padded output length must be a multiple of 4")
      })?

class \nodoc\ iso _TestBase64EncodeURLUnpaddedProperty is UnitTest
  """
  Unpadded url encoding: the output has exactly the length implied by the input
  (with the trailing partial quantum *not* padded out), and contains only
  url-safe characters — no '=' and, critically, no NUL. This is the property
  the old NUL-padding bug violated.
  """
  fun name(): String => "encode/Base64.property/encode_url_unpadded"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) =>
        let enc: String val = Base64.encode_url[String iso](sample, false)
        let n = sample.size()
        let rem =
          match n % 3
          | 1 => USize(2)
          | 2 => USize(3)
          else USize(0)
          end
        ph.assert_eq[USize](((n / 3) * 4) + rem, enc.size(),
          "unpadded url output has the wrong length")
        for b in enc.values() do
          if not _B64.is_url_char(b) then
            ph.fail("unpadded url output contains a non-url-safe byte: "
              + b.string())
            break
          end
        end
      })?

class \nodoc\ iso _TestBase64EncodeURLPaddingProperty is UnitTest
  """
  The unpadded url encoding equals the padded one with its trailing '='
  characters removed. '=' only ever appears as trailing padding, so stripping
  every '=' from the padded form must reproduce the unpadded form exactly.
  """
  fun name(): String => "encode/Base64.property/encode_url_padding"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) =>
        let padded: String val = Base64.encode_url[String iso](sample, true)
        let unpadded: String val = Base64.encode_url[String iso](sample, false)
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
  Oracle: the url-safe alphabet differs from the standard one only at indices
  62 ('+' -> '-') and 63 ('/' -> '_'); padding is identical. So the padded url
  encoding must equal the standard encoding with just those two characters
  translated. This checks encode and encode_url against each other.
  """
  fun name(): String => "encode/Base64.property/encode_url_alphabet_oracle"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) =>
        let std: String val = Base64.encode(sample)
        let url: String val = Base64.encode_url[String iso](sample, true)
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

class \nodoc\ iso _TestBase64EncodeAlphabetProperty is UnitTest
  """
  Every byte of the standard encoding is a member of the RFC 4648 alphabet
  (A-Z, a-z, 0-9, '+', '/') or the '=' pad character — nothing else.
  """
  fun name(): String => "encode/Base64.property/encode_alphabet"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(), h)(
      {(sample, ph) =>
        let enc: String val = Base64.encode(sample)
        for b in enc.values() do
          if not _B64.is_std_char(b) then
            ph.fail("standard output contains a non-base64 byte: " + b.string())
            break
          end
        end
      })?

class \nodoc\ iso _TestBase64WrappedLineLengthProperty is UnitTest
  """
  Line wrapping honours the configured width: no line of a PEM encoding exceeds
  64 characters, and no line of a MIME encoding exceeds 76. This pins the
  linelen argument of the wrapped encoders (a swap with the pad argument would
  produce unwrapped output and fail here).
  """
  fun name(): String => "encode/Base64.property/wrapped_line_length"

  fun apply(h: TestHelper) ? =>
    PonyCheck.for_all[Array[U8]](_B64.bytes(0, 256), h)(
      {(sample, ph) =>
        let pem: String val = Base64.encode_pem(sample)
        let mime: String val = Base64.encode_mime(sample)
        ph.assert_true(_B64.max_line_length(pem) <= 64,
          "PEM produced a line longer than 64 characters")
        ph.assert_true(_B64.max_line_length(mime) <= 76,
          "MIME produced a line longer than 76 characters")
      })?

primitive \nodoc\ _B64
  """
  Shared generator and helpers for the Base64 property tests.
  """
  fun bytes(min: USize = 0, max: USize = 64): Generator[Array[U8]] val =>
    """
    Arbitrary byte arrays. The element generator is mostly uniform but mixes in
    the boundary bytes that matter for Base64 (NUL, 1, 62, 63, 254, 255) so most
    samples exercise them.
    """
    recover val
      Generators.array_of[U8](
        Generators.frequency[U8]([
          as WeightedGenerator[U8]:
          (6, Generators.u8())
          (1, Generators.one_of[U8]([as U8: 0; 1; 62; 63; 254; 255]))
        ]),
        min,
        max)
    end

  fun is_std_char(b: U8): Bool =>
    """A byte of standard RFC 4648 output: the alphabet plus the pad char."""
    ((b >= 'A') and (b <= 'Z')) or
    ((b >= 'a') and (b <= 'z')) or
    ((b >= '0') and (b <= '9')) or
    (b == '+') or (b == '/') or (b == '=')

  fun is_url_char(b: U8): Bool =>
    """
    A byte of unpadded url-safe output: the url alphabet only. Deliberately
    excludes '=' and NUL so it rejects both padding and the old NUL-fill bug.
    """
    ((b >= 'A') and (b <= 'Z')) or
    ((b >= 'a') and (b <= 'z')) or
    ((b >= '0') and (b <= '9')) or
    (b == '-') or (b == '_')

  fun max_line_length(s: String): USize =>
    """
    The length of the longest run of non-CR/LF characters in `s` — i.e. its
    longest line.
    """
    var maxlen = USize(0)
    var run = USize(0)
    for b in s.values() do
      if (b == '\r') or (b == '\n') then
        run = 0
      else
        run = run + 1
        if run > maxlen then maxlen = run end
      end
    end
    maxlen
