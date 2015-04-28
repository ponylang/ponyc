use "ponytest"
use "encode/base64"

actor Main
  new create(env: Env) =>
    var test = PonyTest(env)

    test(recover iso TestBase64 end)
    test(recover iso TestBase64Decode end)

    test.complete()


class TestBase64 iso is UnitTest
  """
  Test base64 encoding.
  Using test examples from RFC 4648.
  """
  fun name():String => "encode.base64"

  fun apply(h: TestHelper): Bool =>
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
  fun name():String => "encode.base64decode"

  fun apply(h: TestHelper): Bool ? =>
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
