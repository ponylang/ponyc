use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestConstantTimeCompare)
    test(_TestMD4)
    test(_TestMD5)
    test(_TestRIPEMD160)
    test(_TestSHA1)
    test(_TestSHA224)
    test(_TestSHA256)
    test(_TestSHA384)
    test(_TestSHA512)
    test(_TestDigest)

class iso _TestConstantTimeCompare is UnitTest
  fun name(): String => "crypto/ConstantTimeCompare"

  fun apply(h: TestHelper) =>
    let s1 = "12345"
    let s2 = "54321"
    let s3 = "123456"
    let s4 = "1234"
    let s5 = recover val [U8(0), U8(0), U8(0), U8(0), U8(0)] end
    let s6 = String.from_array(recover [U8(0), U8(0), U8(0), U8(0), U8(0)] end)
    let s7 = ""
    h.assert_true(ConstantTimeCompare(s1, s1))
    h.assert_false(ConstantTimeCompare(s1, s2))
    h.assert_false(ConstantTimeCompare(s1, s3))
    h.assert_false(ConstantTimeCompare(s1, s4))
    h.assert_false(ConstantTimeCompare(s1, s5))
    h.assert_true(ConstantTimeCompare(s5, s6))
    h.assert_false(ConstantTimeCompare(s1, s6))
    h.assert_false(ConstantTimeCompare(s1, s7))

class iso _TestMD4 is UnitTest
  fun name(): String => "crypto/MD4"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "db346d691d7acc4dc2625db19f9e3f52",
      ToHexString(MD4("test")))

class iso _TestMD5 is UnitTest
  fun name(): String => "crypto/MD5"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "098f6bcd4621d373cade4e832627b4f6",
      ToHexString(MD5("test")))

class iso _TestRIPEMD160 is UnitTest
  fun name(): String => "crypto/RIPEMD160"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "5e52fee47e6b070565f74372468cdc699de89107",
      ToHexString(RIPEMD160("test")))

class iso _TestSHA1 is UnitTest
  fun name(): String => "crypto/SHA1"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
      ToHexString(SHA1("test")))

class iso _TestSHA224 is UnitTest
  fun name(): String => "crypto/SHA224"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "90a3ed9e32b2aaf4c61c410eb925426119e1a9dc53d4286ade99a809",
      ToHexString(SHA224("test")))

class iso _TestSHA256 is UnitTest
  fun name(): String => "crypto/SHA256"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
      ToHexString(SHA256("test")))

class iso _TestSHA384 is UnitTest
  fun name(): String => "crypto/SHA384"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "768412320f7b0aa5812fce428dc4706b3cae50e02a64caa16a782249bfe8efc4b7ef1ccb126255d196047dfedf17a0a9",
      ToHexString(SHA384("test")))

class iso _TestSHA512 is UnitTest
  fun name(): String => "crypto/SHA512"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "ee26b0dd4af7e749aa1a8ee3c10ae9923f618980772e473f8819a5d4940e0db27ac185f8a0e1d5f84f88bc887fd67b143732c304cc5fa9ad8e6f57f50028a8ff",
      ToHexString(SHA512("test")))

class iso _TestDigest is UnitTest
  fun name(): String => "crypto/Digest"

  fun apply(h: TestHelper) ? =>
    let d = Digest.md5()
    d.append("message1")
    d.append("message2")
    h.assert_eq[String](
      "94af09c09bb9bb7b5c94fec6e6121482",
      ToHexString(d.final()))
    h.assert_eq[String](
      "94af09c09bb9bb7b5c94fec6e6121482",
      ToHexString(d.final()))

    let d' = Digest.sha512()
    d'.append("test")
    h.assert_eq[String](
      "ee26b0dd4af7e749aa1a8ee3c10ae9923f618980772e473f8819a5d4940e0db27ac185f8a0e1d5f84f88bc887fd67b143732c304cc5fa9ad8e6f57f50028a8ff",
      ToHexString(d'.final()))
