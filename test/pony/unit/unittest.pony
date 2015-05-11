use "ponytest"
use "collections"
use "encode/base64"

actor Main
  new create(env: Env) =>
    var test = PonyTest(env)

    test(recover TestBase64 end)
    test(recover TestBase64Decode end)
    test(recover TestStringRunes end)

    test.complete()


class TestBase64 iso is UnitTest
  """
  Test base64 encoding.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode.base64"

  fun apply(h: TestHelper): TestResult =>
    h.expect_eq[String]("", Base64.encode(""))
    h.expect_eq[String]("Zg==", Base64.encode("f"))
    h.expect_eq[String]("Zm8=", Base64.encode("fo"))
    h.expect_eq[String]("Zm9v", Base64.encode("foo"))
    h.expect_eq[String]("Zm9vYg==", Base64.encode("foob"))
    h.expect_eq[String]("Zm9vYmE=", Base64.encode("fooba"))
    h.expect_eq[String]("Zm9vYmFy", Base64.encode("foobar"))
    true


class TestBase64Decode iso is UnitTest
  """
  Test base64 decoding. Examples with and without padding are tested.
  Using test examples from RFC 4648.
  """
  fun name(): String => "encode.base64decode"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[String]("", Base64.decode[String iso](""))
    h.expect_eq[String]("f", Base64.decode[String iso]("Zg=="))
    h.expect_eq[String]("fo", Base64.decode[String iso]("Zm8="))
    h.expect_eq[String]("foo", Base64.decode[String iso]("Zm9v"))
    h.expect_eq[String]("foob", Base64.decode[String iso]("Zm9vYg=="))
    h.expect_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE="))
    h.expect_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy"))

    h.expect_eq[String]("", Base64.decode[String iso](""))
    h.expect_eq[String]("f", Base64.decode[String iso]("Zg"))
    h.expect_eq[String]("fo", Base64.decode[String iso]("Zm8"))
    h.expect_eq[String]("foo", Base64.decode[String iso]("Zm9v"))
    h.expect_eq[String]("foob", Base64.decode[String iso]("Zm9vYg"))
    h.expect_eq[String]("fooba", Base64.decode[String iso]("Zm9vYmE"))
    h.expect_eq[String]("foobar", Base64.decode[String iso]("Zm9vYmFy"))
    true

class TestStringRunes iso is UnitTest
  """
  Test iterating over the unicode codepoints in a string.
  """
  fun name(): String => "string.runes"

  fun apply(h: TestHelper): TestResult ? =>
    let s = "\u16ddx\ufb04"
    let expect = [as U32: 0x16dd, 'x', 0xfb04]
    let result = Array[U32]

    for c in s.runes() do
      result.push(c)
    end

    h.expect_eq[U64](expect.size(), result.size())

    for i in Range(0, expect.size()) do
      h.expect_eq[U32](expect(i), result(i))
    end

    true
