use "pony_test"

use @memset[Pointer[None]](dst: Pointer[None], value: I32, n: USize)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

actor \nodoc\ Main is TestList
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
    test(_TestDigestFinalTwice)
    test(_TestDigestAppendAfterFinal)
    test(_TestDigestDroppedWithoutFinal)
    test(_TestDigestDroppedAfterFinal)
    test(_TestHmacSha256Rfc4231)
    test(_TestHmacSha256Scram)
    test(_TestRandBytes)
    test(_TestHmacSha256EmptyMessage)
    test(_TestRandBytesSizeTooLarge)
    test(_TestPbkdf2Sha256ArgumentsTooLarge)
    test(PropertyTest[USize](_TestHmacSha256OutputLength))
    test(PropertyTest[USize](_TestHmacSha256Deterministic))
    test(PropertyTest[USize](_TestRandBytesOutputLength))
    test(PropertyTest[USize](_TestRandBytesNonConstant))
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      test(_TestPbkdf2Sha256Rfc7914)
      test(_TestPbkdf2Sha256Scram)
      test(PropertyTest[USize](_TestPbkdf2Sha256OutputLength))
      test(PropertyTest[USize](_TestPbkdf2Sha256Deterministic))
    end
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      test(_TestShake128KnownAnswer)
      test(_TestShake256KnownAnswer)
      test(PropertyTest[USize](_TestShake128XofPrefixSmall))
      test(PropertyTest[USize](_TestShake256XofPrefixSmall))
      test(PropertyTest[USize](_TestShake128XofPrefix))
      test(PropertyTest[USize](_TestShake256XofPrefix))
    end
    test(_TestHashFnBoxReceiver)
    test(PropertyTest[USize](_TestHashFnOutputLength))
    test(PropertyTest[USize](_TestHashFnDeterministic))
    test(PropertyTest[USize](_TestHashFnDigestEquivalence))
    test(PropertyTest[(USize, USize)](_TestDigestConcatenation))
    test(PropertyTest[USize](_TestDigestOutputLength))
    test(PropertyTest[USize](_TestConstantTimeCompareReflexive))
    test(PropertyTest[(USize, USize)](_TestConstantTimeCompareSensitive))
    test(PropertyTest[USize](_TestToHexStringLength))
    test(PropertyTest[U8](_TestToHexStringValidHex))

class \nodoc\ iso _TestConstantTimeCompare is UnitTest
  fun name(): String => "crypto/ConstantTimeCompare"

  fun apply(h: TestHelper) =>
    let s1 = "12345"
    let s2 = "54321"
    let s3 = "123456"
    let s4 = "1234"
    let s5 = recover val [ as U8: 0; 0; 0; 0; 0] end
    let s6 = String.from_array([0; 0; 0; 0; 0])
    let s7 = ""
    h.assert_true(ConstantTimeCompare(s1, s1))
    h.assert_false(ConstantTimeCompare(s1, s2))
    h.assert_false(ConstantTimeCompare(s1, s3))
    h.assert_false(ConstantTimeCompare(s1, s4))
    h.assert_false(ConstantTimeCompare[ByteSeq box](s1, s5))
    h.assert_true(ConstantTimeCompare[ByteSeq box](s5, s6))
    h.assert_false(ConstantTimeCompare(s1, s6))
    h.assert_false(ConstantTimeCompare(s1, s7))

class \nodoc\ iso _TestMD4 is UnitTest
  fun name(): String => "crypto/MD4"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "db346d691d7acc4dc2625db19f9e3f52",
      ToHexString(MD4("test")))

class \nodoc\ iso _TestMD5 is UnitTest
  fun name(): String => "crypto/MD5"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "098f6bcd4621d373cade4e832627b4f6",
      ToHexString(MD5("test")))

class \nodoc\ iso _TestRIPEMD160 is UnitTest
  fun name(): String => "crypto/RIPEMD160"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "5e52fee47e6b070565f74372468cdc699de89107",
      ToHexString(RIPEMD160("test")))

class \nodoc\ iso _TestSHA1 is UnitTest
  fun name(): String => "crypto/SHA1"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
      ToHexString(SHA1("test")))

class \nodoc\ iso _TestSHA224 is UnitTest
  fun name(): String => "crypto/SHA224"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "90a3ed9e32b2aaf4c61c410eb925426119e1a9dc53d4286ade99a809",
      ToHexString(SHA224("test")))

class \nodoc\ iso _TestSHA256 is UnitTest
  fun name(): String => "crypto/SHA256"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
      ToHexString(SHA256("test")))

class \nodoc\ iso _TestSHA384 is UnitTest
  fun name(): String => "crypto/SHA384"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "768412320f7b0aa5812fce428dc4706b3cae50e02a64caa16a782249bfe8efc4" +
      "b7ef1ccb126255d196047dfedf17a0a9",
      ToHexString(SHA384("test")))

class \nodoc\ iso _TestSHA512 is UnitTest
  fun name(): String => "crypto/SHA512"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "ee26b0dd4af7e749aa1a8ee3c10ae9923f618980772e473f8819a5d4940e0db2" +
      "7ac185f8a0e1d5f84f88bc887fd67b143732c304cc5fa9ad8e6f57f50028a8ff",
      ToHexString(SHA512("test")))

class \nodoc\ iso _TestDigest is UnitTest
  fun name(): String => "crypto/Digest"

  fun apply(h: TestHelper) ? =>
    let md5 = Digest.md5()?
    md5.append("message1")?
    md5.append("message2")?
    h.assert_eq[String](
      "94af09c09bb9bb7b5c94fec6e6121482",
      ToHexString(md5.final()?))

    let sha1 = Digest.sha1()?
    sha1.append("message1")?
    sha1.append("message2")?
    h.assert_eq[String](
      "942682e2e49f37b4b224fc1aec1a53a25967e7e0",
      ToHexString(sha1.final()?))

    let sha224 = Digest.sha224()?
    sha224.append("message1")?
    sha224.append("message2")?
    h.assert_eq[String](
      "fbba013f116e8b09b044b2a785ed7fb6a65ce672d724c1fb20500d45",
      ToHexString(sha224.final()?))

    let sha256 = Digest.sha256()?
    sha256.append("message1")?
    sha256.append("message2")?
    h.assert_eq[String](
      "68d9b867db4bde630f3c96270b2320a31a72aafbc39997eb2bc9cf2da21e5213",
      ToHexString(sha256.final()?))

    let sha384 = Digest.sha384()?
    sha384.append("message1")?
    sha384.append("message2")?
    h.assert_eq[String](
      "7736dd67494a7072bf255852bd327406b398cb0b16cb400fcd3fcfb6827d74ab" +
      "9b14673d50515b6273ef15543325f8d3",
      ToHexString(sha384.final()?))

    let sha512 = Digest.sha512()?
    sha512.append("message1")?
    sha512.append("message2")?
    h.assert_eq[String](
      "3511f4825021a90cd55d37db5c3250e6bbcffc9a0d56d88b4e2878ac5b094692" +
      "cd945c6a77006272322f911c9be31fa970043daa4b61cee607566cbfa2c69b09",
      ToHexString(sha512.final()?))

    let ripemd160 = Digest.ripemd160()?
    ripemd160.append("message1")?
    ripemd160.append("message2")?
    h.assert_eq[String](
      "9813627fca6c51a67ed401cf325c3864cb84ce34",
      ToHexString(ripemd160.final()?))

    ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
      let shake128 = Digest.shake128()?
      shake128.append("message1")?
      shake128.append("message2")?
      h.assert_eq[String](
        "0d11671f23b6356bdf4ba8dcae37419d",
        ToHexString(shake128.final()?))

      let shake256 = Digest.shake256()?
      shake256.append("message1")?
      shake256.append("message2")?
      h.assert_eq[String](
        "80e2bbb14639e3b1fc1df80b47b67fb518b0ed26a1caddfa10d68f7992c33820",
        ToHexString(shake256.final()?))
    end

class \nodoc\ iso _TestDigestFinalTwice is UnitTest
  """
  A second `final` returns the hash the first one made.

  The first call gave the context back to OpenSSL and left a null pointer in its
  place. A second call that recomputed the hash would hand that null to
  `EVP_DigestFinal_ex`.
  """
  fun name(): String => "crypto/Digest/final_twice"

  fun apply(h: TestHelper) ? =>
    let expected =
      "68d9b867db4bde630f3c96270b2320a31a72aafbc39997eb2bc9cf2da21e5213"

    let digest = Digest.sha256()?
    digest.append("message1")?
    digest.append("message2")?

    h.assert_eq[String](expected, ToHexString(digest.final()?))
    h.assert_eq[String](expected, ToHexString(digest.final()?))

class \nodoc\ iso _TestDigestAppendAfterFinal is UnitTest
  """
  `append` raises an error once `final` has been called.

  A finalized digest holds a null pointer where its context was. `append` raises
  because the hash is already made, and that is what keeps the null out of
  `EVP_DigestUpdate`.
  """
  fun name(): String => "crypto/Digest/append_after_final"

  fun apply(h: TestHelper) ? =>
    let digest = Digest.sha256()?
    digest.append("message1")?
    digest.final()?

    let raised =
      try
        digest.append("message2")?
        false
      else
        true
      end

    h.assert_true(raised, "append after final should raise an error")

class \nodoc\ _TestDigestHolder
  """
  Holds a digest and reports its own collection by writing a byte through
  `_collected`, a raw pointer into an array a live `_TestDigestDropRunner`
  holds.
  """
  let _collected: Pointer[U8] tag
  // Nothing reads this. Holding the digest is the point: nothing else refers to
  // it, so the sweep that finalizes this object finalizes the digest too.
  let _digest: Digest

  new create(collected: Pointer[U8] tag, digest: Digest) =>
    _collected = collected
    _digest = digest

  fun _final() =>
    @memset(_collected, I32(1), USize(1))

actor \nodoc\ _TestDigestDropRunner
  """
  Drops a digest, forces a collection, and reports whether the collector reached
  it. Pony collects between behaviors, so the drop happens in one behavior and
  the check in the next.
  """
  let _h: TestHelper
  let _finalize: Bool
  embed _flag: Array[U8] = Array[U8].init(0, 1)

  new create(h: TestHelper, finalize: Bool) =>
    _h = h
    _finalize = finalize

  be run() =>
    // The digest and the holder are both locals. Once this behavior returns
    // nothing roots either of them.
    try
      let digest = Digest.sha256()?
      digest.append("message1")?
      if _finalize then
        digest.final()?
      end
      _TestDigestHolder(_flag.cpointer(), digest)
    else
      _h.fail("could not append to a digest")
      _h.complete(false)
      return
    end

    @pony_triggergc(@pony_ctx())
    _check()

  be _check() =>
    _h.assert_true(
      // A one element array cannot fail to index. If it somehow did, fail
      // rather than pass by accident.
      try _flag(0)? != 0 else false end,
      "a dropped digest should be collected")
    _h.complete(true)

class \nodoc\ iso _TestDigestDroppedWithoutFinal is UnitTest
  """
  A digest dropped without a call to `final()` reaches the collector, and
  `Digest._final` runs on a context nothing has freed.

  `_final` cannot be called directly, so the digest has to be collected. It is
  held by an object that reports its own collection, and nothing else refers to
  either of them, so the sweep that finalizes the holder finalizes the digest.

  What `_final` hands back to OpenSSL is invisible from Pony, so this cannot
  watch the context being freed. It can only run the free, and run it here,
  where a `_final` that gives OpenSSL a pointer it should not have kills this
  test rather than something further along.
  """
  fun name(): String => "crypto/Digest/dropped_without_final"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestDigestDropRunner(h where finalize = false).run()

class \nodoc\ iso _TestDigestDroppedAfterFinal is UnitTest
  """
  A digest collected after `final()` does not hand OpenSSL its context a second
  time.

  `final()` frees the context and leaves a null pointer behind, and `_final`
  frees nothing when it finds one. A `final()` that left the freed pointer in
  place would give it to `EVP_MD_CTX_free` again when the collector arrived.
  """
  fun name(): String => "crypto/Digest/dropped_after_final"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestDigestDropRunner(h where finalize = true).run()

class \nodoc\ iso _TestHmacSha256Rfc4231 is UnitTest
  """
  HMAC-SHA-256 test vectors from RFC 4231.
  """
  fun name(): String => "crypto/HmacSha256/RFC4231"

  fun apply(h: TestHelper) ? =>
    // Test Case 1: Key = 20 bytes of 0x0b, Data = "Hi There"
    let key1 =
      recover val
        let a = Array[U8].create(20)
        var i: USize = 0
        while i < 20 do a.push(0x0b); i = i + 1 end
        a
      end
    h.assert_eq[String](
      "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
      ToHexString(HmacSha256(key1, "Hi There")?))

    // Test Case 2: Key = "Jefe", Data = "what do ya want for nothing?"
    h.assert_eq[String](
      "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
      ToHexString(HmacSha256("Jefe", "what do ya want for nothing?")?))

    // Test Case 3: Key = 20 bytes of 0xaa, Data = 50 bytes of 0xdd
    let key3 =
      recover val
        let a = Array[U8].create(20)
        var i: USize = 0
        while i < 20 do a.push(0xaa); i = i + 1 end
        a
      end
    let data3 =
      recover val
        let a = Array[U8].create(50)
        var i: USize = 0
        while i < 50 do a.push(0xdd); i = i + 1 end
        a
      end
    h.assert_eq[String](
      "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
      ToHexString(HmacSha256(key3, data3)?))

    // Test Case 4: Key = 0x01..0x19, Data = 50 bytes of 0xcd
    let key4 =
      recover val
        let a = Array[U8].create(25)
        var i: U8 = 0x01
        while i <= 0x19 do a.push(i); i = i + 1 end
        a
      end
    let data4 =
      recover val
        let a = Array[U8].create(50)
        var i: USize = 0
        while i < 50 do a.push(0xcd); i = i + 1 end
        a
      end
    h.assert_eq[String](
      "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
      ToHexString(HmacSha256(key4, data4)?))

    // Test Case 5: Truncation to 128 bits
    let key5 =
      recover val
        let a = Array[U8].create(20)
        var i: USize = 0
        while i < 20 do a.push(0x0c); i = i + 1 end
        a
      end
    let hmac5 = HmacSha256(key5, "Test With Truncation")?
    h.assert_eq[String](
      "a3b6167473100ee06e0c796c2955552b",
      ToHexString(recover val
        let a = Array[U8].create(16)
        var i: USize = 0
        while i < 16 do
          try a.push(hmac5(i)?) end
          i = i + 1
        end
        a
      end))

    // Test Case 6: Key = 131 bytes of 0xaa, large key
    let key6 =
      recover val
        let a = Array[U8].create(131)
        var i: USize = 0
        while i < 131 do a.push(0xaa); i = i + 1 end
        a
      end
    h.assert_eq[String](
      "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
      ToHexString(
        HmacSha256(
          key6, "Test Using Larger Than Block-Size Key - Hash Key First")?))

    // Test Case 7: Key = 131 bytes of 0xaa, large key + large data
    h.assert_eq[String](
      "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
      ToHexString(
        HmacSha256(
          key6,
          "This is a test using a larger than block-size key and a larger " +
          "than block-size data. The key needs to be hashed before being " +
          "used by the HMAC algorithm.")?))

class \nodoc\ iso _TestHmacSha256Scram is UnitTest
  """
  HMAC-SHA-256 with SCRAM-SHA-256 intermediate values derived from the
  RFC 7677 protocol exchange. Verified against the RFC's ClientProof and
  ServerSignature.
  """
  fun name(): String => "crypto/HmacSha256/SCRAM"

  fun apply(h: TestHelper) ? =>
    // SaltedPassword from PBKDF2("pencil", salt, 4096, 32)
    let salted_password =
      recover val
        [ as U8:
          0xc4; 0xa4; 0x95; 0x10; 0x32; 0x3a; 0xb4; 0xf9
          0x52; 0xca; 0xc1; 0xfa; 0x99; 0x44; 0x19; 0x39
          0xe7; 0x8e; 0xa7; 0x4d; 0x6b; 0xe8; 0x1d; 0xdf
          0x70; 0x96; 0xe8; 0x75; 0x13; 0xdc; 0x61; 0x5d
        ]
      end

    // HMAC(SaltedPassword, "Client Key")
    h.assert_eq[String](
      "a60fc923d67e8644a92d16b96eda5ef4656b0c725c484374be25535576996e8b",
      ToHexString(HmacSha256(salted_password, "Client Key")?))

    // HMAC(SaltedPassword, "Server Key")
    h.assert_eq[String](
      "c1f3cbc1c13a9d35a14c0990eed97629ea225863e566a4314ab99f3f00e5d9d5",
      ToHexString(HmacSha256(salted_password, "Server Key")?))

class \nodoc\ iso _TestPbkdf2Sha256Rfc7914 is UnitTest
  """
  PBKDF2-HMAC-SHA256 test vectors from RFC 7914, Section 11.
  """
  fun name(): String => "crypto/Pbkdf2Sha256/RFC7914"

  fun apply(h: TestHelper) ? =>
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      // Test Vector 1: Password "passwd", Salt "salt", c=1, dkLen=64
      h.assert_eq[String](
        "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc" +
        "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783",
        ToHexString(Pbkdf2Sha256("passwd", "salt", 1, 64)?))

      // Test Vector 2: Password "Password", Salt "NaCl", c=80000, dkLen=64
      h.assert_eq[String](
        "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56" +
        "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d",
        ToHexString(Pbkdf2Sha256("Password", "NaCl", 80000, 64)?))
    end

class \nodoc\ iso _TestPbkdf2Sha256Scram is UnitTest
  """
  PBKDF2-HMAC-SHA256 with the SCRAM-SHA-256 test vector from RFC 7677.
  The SaltedPassword is derived from the RFC 7677 protocol exchange and
  verified against the RFC's ClientProof and ServerSignature.
  """
  fun name(): String => "crypto/Pbkdf2Sha256/SCRAM"

  fun apply(h: TestHelper) ? =>
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      // Salt: decoded bytes of base64 "W22ZaJ0SNY7soEsUEjb6gQ=="
      let salt =
        recover val
          [ as U8:
            0x5b; 0x6d; 0x99; 0x68; 0x9d; 0x12; 0x35; 0x8e
            0xec; 0xa0; 0x4b; 0x14; 0x12; 0x36; 0xfa; 0x81
          ]
        end

      h.assert_eq[String](
        "c4a49510323ab4f952cac1fa99441939e78ea74d6be81ddf7096e87513dc615d",
        ToHexString(Pbkdf2Sha256("pencil", salt, 4096, 32)?))
    end

class \nodoc\ iso _TestRandBytes is UnitTest
  fun name(): String => "crypto/RandBytes"

  fun apply(h: TestHelper) ? =>
    // Zero-length request
    let r0 = RandBytes(0)?
    h.assert_eq[USize](0, r0.size())

    // Single byte
    let r1 = RandBytes(1)?
    h.assert_eq[USize](1, r1.size())

    // 32 bytes
    let r32 = RandBytes(32)?
    h.assert_eq[USize](32, r32.size())

class \nodoc\ iso _TestHmacSha256EmptyMessage is UnitTest
  """
  A code over an empty key or an empty message is the code of that key and
  message, not thirty-two zero bytes.

  An empty `Array[U8]` has a null pointer, and `HMAC` returns NULL when both the
  key and the message pointers are null. Without the fix, `HmacSha256` asked
  OpenSSL for a code over an empty key and an empty message, got nothing back,
  and returned its zeroed output buffer as the code.

  An empty `String` has a pointer, so the same call written with `""` gives the
  right answer either way. `Array[U8]` is what a caller holds after decoding a
  zero-length field off the wire.
  """
  fun name(): String => "crypto/HmacSha256/empty_message"

  fun apply(h: TestHelper) ? =>
    let empty = recover val Array[U8] end

    h.assert_eq[String](
      "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad",
      ToHexString(HmacSha256(empty, empty)?),
      "an empty key and an empty message")
    h.assert_eq[String](
      "5f576a8d68fe4fb7eb823227246353c0870c3b0e878997341db1226b4bd88d61",
      ToHexString(HmacSha256(empty, "msg")?),
      "an empty key")
    h.assert_eq[String](
      "5d5d139563c95b5967b9bd9a8c9b233a9dedb45072794cd232dc1b74832607d0",
      ToHexString(HmacSha256("key", empty)?),
      "an empty message")

class \nodoc\ iso _TestRandBytesSizeTooLarge is UnitTest
  """
  `RandBytes` raises rather than narrowing a size that OpenSSL's `int` cannot
  hold.

  `RAND_bytes` takes an `int`. A size of `2^32 + 5` narrows to 5: without the
  check the array is the full size, `RAND_bytes` fills five bytes of it and
  reports success, and the caller gets a nonce that is four gigabytes of zeros
  with five random bytes in it.

  A size between `2^31` and `2^32` narrows to a negative `int`, which
  `RAND_bytes` rejects on its own. Those sizes raise with or without the check
  and say nothing about it.

  The check runs before the array is allocated, so this test allocates nothing.
  """
  fun name(): String => "crypto/RandBytes/size_too_large"

  fun apply(h: TestHelper) =>
    ifdef lp64 or llp64 then
      h.assert_error(
        {()? => RandBytes(4294967301)? },
        "a size whose low 32 bits are small should raise")
    end

    h.assert_error(
      {()? => RandBytes(I32.max_value().usize() + 1)? },
      "a size larger than an int should raise")

    try
      h.assert_eq[USize](32, RandBytes(32)?.size(), "a sane size still works")
    else
      h.fail("RandBytes should not raise on a sane size")
    end

class \nodoc\ iso _TestPbkdf2Sha256ArgumentsTooLarge is UnitTest
  """
  `Pbkdf2Sha256` raises rather than narrowing an argument that OpenSSL's `int`
  cannot hold.

  A key length of `2^32 + 5` narrows to 5: without the check the array is the
  full length, `PKCS5_PBKDF2_HMAC` writes five bytes of it and reports success,
  and the rest is zeros.

  The iteration count has no such band. Every `U32` too large for an `int`
  narrows to a negative one, and OpenSSL 3.x rejects that on its own, so the
  assertion below holds with or without the check. It is here because the four
  backends do not agree on what they do with a negative iteration count, and
  this package should not have to know which one it is talking to.

  Each check runs before the key array is allocated.
  """
  fun name(): String => "crypto/Pbkdf2Sha256/arguments_too_large"

  fun apply(h: TestHelper) =>
    ifdef lp64 or llp64 then
      h.assert_error(
        {()? => Pbkdf2Sha256("password", "salt", 4096, 4294967301)? },
        "a key length whose low 32 bits are small should raise")
    end

    h.assert_error(
      {()? =>
        Pbkdf2Sha256("password", "salt", I32.max_value().u32() + 1, 32)?
      },
      "an iteration count larger than an int should raise")

    try
      h.assert_eq[USize](
        32,
        Pbkdf2Sha256("password", "salt", 4096, 32)?.size(),
        "sane arguments still work")
    else
      h.fail("Pbkdf2Sha256 should not raise on sane arguments")
    end

class \nodoc\ iso _TestHmacSha256OutputLength is Property[USize]
  fun name(): String => "crypto/HmacSha256/property/output_length"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    let key = recover val Array[U8].init(0x42, sample) end
    let data = recover val Array[U8].init(0xAB, sample) end
    h.assert_eq[USize](32, HmacSha256(key, data)?.size())

class \nodoc\ iso _TestHmacSha256Deterministic is Property[USize]
  fun name(): String => "crypto/HmacSha256/property/deterministic"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    let key = recover val Array[U8].init(0x42, sample) end
    let data = recover val Array[U8].init(0xAB, sample) end
    h.assert_array_eq[U8](HmacSha256(key, data)?, HmacSha256(key, data)?)

class \nodoc\ iso _TestPbkdf2Sha256OutputLength is Property[USize]
  fun name(): String => "crypto/Pbkdf2Sha256/property/output_length"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 128)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      h.assert_eq[USize](
        sample, Pbkdf2Sha256("p", "s", 1, sample)?.size())
    end

class \nodoc\ iso _TestPbkdf2Sha256Deterministic is Property[USize]
  fun name(): String => "crypto/Pbkdf2Sha256/property/deterministic"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 64)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      h.assert_array_eq[U8](
        Pbkdf2Sha256("p", "s", 1, sample)?,
        Pbkdf2Sha256("p", "s", 1, sample)?)
    end

class \nodoc\ iso _TestRandBytesOutputLength is Property[USize]
  fun name(): String => "crypto/RandBytes/property/output_length"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    h.assert_eq[USize](sample, RandBytes(sample)?.size())

class \nodoc\ iso _TestRandBytesNonConstant is Property[USize]
  fun name(): String => "crypto/RandBytes/property/non_constant"

  fun gen(): Generator[USize] =>
    Generators.usize(16, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    let a = RandBytes(sample)?
    let b = RandBytes(sample)?
    h.assert_false(ConstantTimeCompare(a, b))

class \nodoc\ iso _TestShake128KnownAnswer is UnitTest
  """
  Known-answer tests for SHAKE128 at two non-default output lengths.
  Anchors the XOF path against specific byte values so a silent no-op
  (zero bytes written) cannot pass.
  """
  fun name(): String => "crypto/Shake128/known_answer"

  fun apply(h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let d32 = Digest.shake128(32)?
      d32.append("message1")?
      d32.append("message2")?
      h.assert_eq[String](
        "0d11671f23b6356bdf4ba8dcae37419df1d0875e1a15c7859eb3ba0096aa262f",
        ToHexString(d32.final()?))

      let d64 = Digest.shake128(64)?
      d64.append("message1")?
      d64.append("message2")?
      h.assert_eq[String](
        "0d11671f23b6356bdf4ba8dcae37419df1d0875e1a15c7859eb3ba0096aa262f" +
        "3a1cfc86db5b324ac3f8220645ec0740c2171a0b935f362d0c3bfa5ab51be5d0",
        ToHexString(d64.final()?))
    end

class \nodoc\ iso _TestShake256KnownAnswer is UnitTest
  """
  Known-answer tests for SHAKE256 at two non-default output lengths.
  Anchors the XOF path against specific byte values so a silent no-op
  (zero bytes written) cannot pass.
  """
  fun name(): String => "crypto/Shake256/known_answer"

  fun apply(h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let d64 = Digest.shake256(64)?
      d64.append("message1")?
      d64.append("message2")?
      h.assert_eq[String](
        "80e2bbb14639e3b1fc1df80b47b67fb518b0ed26a1caddfa10d68f7992c33820" +
        "2d0b17a5ebbcef93f51247497f60bcd3f2809a967874d017ef5b51d6b08836cc",
        ToHexString(d64.final()?))

      let d128 = Digest.shake256(128)?
      d128.append("message1")?
      d128.append("message2")?
      h.assert_eq[String](
        "80e2bbb14639e3b1fc1df80b47b67fb518b0ed26a1caddfa10d68f7992c33820" +
        "2d0b17a5ebbcef93f51247497f60bcd3f2809a967874d017ef5b51d6b08836cc" +
        "af79f3db3fafdf89e7d42270472c3d1a8e55c52a30859e01b5fceba359c21c1e" +
        "76b73180378604d46061c87e65c4740c8ff9721ed16465cef66fefc3c6f2070c",
        ToHexString(d128.final()?))
    end

class \nodoc\ iso _TestShake128XofPrefixSmall is Property[USize]
  """
  SHAKE128 prefix property at small output sizes (2..15 bytes). Exercises
  the truncation path where off-by-one partial-block bugs typically live.

  Both results are checked against a known answer as well. Prefix equality on
  its own holds for an implementation that writes nothing.
  """
  fun name(): String => "crypto/Shake128/property/xof_prefix_small"

  fun gen(): Generator[USize] =>
    Generators.usize(2, 15)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let small_size = sample / 2
      let large_size = sample

      let small = Digest.shake128(small_size)?
      small.append("test input")?
      let small_result = small.final()?

      let large = Digest.shake128(large_size)?
      large.append("test input")?
      let large_result = large.final()?

      h.assert_array_eq[U8](
        small_result, large_result.trim(0, small_size))

      // The first 16 bytes of SHAKE128("test input"). The generator stops at
      // 15, so every size it produces is inside the anchor.
      let kat = "a9d2b0362d0e2e961eeb969ce9a42f2d"
      h.assert_eq[String](
        kat.substring(0, (small_size * 2).isize()),
        ToHexString(small_result))
      h.assert_eq[String](
        kat.substring(0, (large_size * 2).isize()),
        ToHexString(large_result))
    end

class \nodoc\ iso _TestShake128XofPrefix is Property[USize]
  """
  SHAKE128 prefix property: the first N bytes of output at length M (M > N)
  are identical to the full output at length N. A KAT anchor at the full
  length prevents any no-op or incorrect XOF implementation from satisfying
  the prefix equality (an all-zero buffer would trivially equal its own
  prefix).
  """
  fun name(): String => "crypto/Shake128/property/xof_prefix"

  fun gen(): Generator[USize] =>
    // Minimum 16 so the 16-byte KAT anchor below always applies.
    Generators.usize(16, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let small_size = sample / 2
      let large_size = sample

      let small = Digest.shake128(small_size)?
      small.append("test input")?
      let small_result = small.final()?

      let large = Digest.shake128(large_size)?
      large.append("test input")?
      let large_result = large.final()?

      h.assert_array_eq[U8](
        small_result, large_result.trim(0, small_size))

      // KAT anchor: first 16 bytes of SHAKE128("test input").
      h.assert_eq[String](
        "a9d2b0362d0e2e961eeb969ce9a42f2d",
        ToHexString(large_result.trim(0, 16)))
    end

class \nodoc\ iso _TestShake256XofPrefixSmall is Property[USize]
  """
  SHAKE256 prefix property at small output sizes (2..31 bytes). Exercises
  the truncation path where off-by-one partial-block bugs typically live.

  Both results are checked against a known answer as well. Prefix equality on
  its own holds for an implementation that writes nothing.
  """
  fun name(): String => "crypto/Shake256/property/xof_prefix_small"

  fun gen(): Generator[USize] =>
    Generators.usize(2, 31)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let small_size = sample / 2
      let large_size = sample

      let small = Digest.shake256(small_size)?
      small.append("test input")?
      let small_result = small.final()?

      let large = Digest.shake256(large_size)?
      large.append("test input")?
      let large_result = large.final()?

      h.assert_array_eq[U8](
        small_result, large_result.trim(0, small_size))

      // The first 32 bytes of SHAKE256("test input"). The generator stops at
      // 31, so every size it produces is inside the anchor.
      let kat =
        "e952d90136cb23413ff22b266e2f5dd42294a34bc311394b04863c039011f179"
      h.assert_eq[String](
        kat.substring(0, (small_size * 2).isize()),
        ToHexString(small_result))
      h.assert_eq[String](
        kat.substring(0, (large_size * 2).isize()),
        ToHexString(large_result))
    end

class \nodoc\ iso _TestShake256XofPrefix is Property[USize]
  """
  SHAKE256 prefix property: the first N bytes of output at length M (M > N)
  are identical to the full output at length N. A KAT anchor at the full
  length prevents any no-op or incorrect XOF implementation from satisfying
  the prefix equality (an all-zero buffer would trivially equal its own
  prefix).
  """
  fun name(): String => "crypto/Shake256/property/xof_prefix"

  fun gen(): Generator[USize] =>
    // Minimum 32 so the 32-byte KAT anchor below always applies.
    Generators.usize(32, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      let small_size = sample / 2
      let large_size = sample

      let small = Digest.shake256(small_size)?
      small.append("test input")?
      let small_result = small.final()?

      let large = Digest.shake256(large_size)?
      large.append("test input")?
      let large_result = large.final()?

      h.assert_array_eq[U8](
        small_result, large_result.trim(0, small_size))

      // KAT anchor: first 32 bytes of SHAKE256("test input").
      h.assert_eq[String](
        "e952d90136cb23413ff22b266e2f5dd42294a34bc311394b04863c039011f179",
        ToHexString(large_result.trim(0, 32)))
    end

primitive \nodoc\ _BoxReceiverHashFn is HashFn
  fun apply(input: ByteSeq): Array[U8] val =>
    recover val Array[U8].init(0x2A, input.size()) end

class \nodoc\ iso _TestHashFnBoxReceiver is UnitTest
  """
  `HashFn.apply` takes a `box` receiver, so a primitive whose `apply` is a
  plain `fun` satisfies the interface and a `HashFn val` can call it.
  """
  fun name(): String => "crypto/HashFn/box_receiver"

  fun apply(h: TestHelper) =>
    let f: HashFn val = _BoxReceiverHashFn
    h.assert_array_eq[U8]([as U8: 0x2A; 0x2A; 0x2A], f("abc"))

class \nodoc\ iso _TestHashFnOutputLength is Property[USize]
  fun name(): String => "crypto/HashFn/property/output_length"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) =>
    let input = recover val Array[U8].init(0x42, sample) end
    h.assert_eq[USize](16, MD4(input).size())
    h.assert_eq[USize](16, MD5(input).size())
    h.assert_eq[USize](20, RIPEMD160(input).size())
    h.assert_eq[USize](20, SHA1(input).size())
    h.assert_eq[USize](28, SHA224(input).size())
    h.assert_eq[USize](32, SHA256(input).size())
    h.assert_eq[USize](48, SHA384(input).size())
    h.assert_eq[USize](64, SHA512(input).size())

class \nodoc\ iso _TestHashFnDeterministic is Property[USize]
  fun name(): String => "crypto/HashFn/property/deterministic"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) =>
    let input = recover val Array[U8].init(0x42, sample) end
    h.assert_array_eq[U8](MD4(input), MD4(input))
    h.assert_array_eq[U8](MD5(input), MD5(input))
    h.assert_array_eq[U8](RIPEMD160(input), RIPEMD160(input))
    h.assert_array_eq[U8](SHA1(input), SHA1(input))
    h.assert_array_eq[U8](SHA224(input), SHA224(input))
    h.assert_array_eq[U8](SHA256(input), SHA256(input))
    h.assert_array_eq[U8](SHA384(input), SHA384(input))
    h.assert_array_eq[U8](SHA512(input), SHA512(input))

class \nodoc\ iso _TestHashFnDigestEquivalence is Property[USize]
  fun name(): String => "crypto/HashFn/property/digest_equivalence"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    let input = recover val Array[U8].init(0x42, sample) end

    let md5 = Digest.md5()?
    md5.append(input)?
    h.assert_array_eq[U8](MD5(input), md5.final()?)

    let ripemd160 = Digest.ripemd160()?
    ripemd160.append(input)?
    h.assert_array_eq[U8](RIPEMD160(input), ripemd160.final()?)

    let sha1 = Digest.sha1()?
    sha1.append(input)?
    h.assert_array_eq[U8](SHA1(input), sha1.final()?)

    let sha224 = Digest.sha224()?
    sha224.append(input)?
    h.assert_array_eq[U8](SHA224(input), sha224.final()?)

    let sha256 = Digest.sha256()?
    sha256.append(input)?
    h.assert_array_eq[U8](SHA256(input), sha256.final()?)

    let sha384 = Digest.sha384()?
    sha384.append(input)?
    h.assert_array_eq[U8](SHA384(input), sha384.final()?)

    let sha512 = Digest.sha512()?
    sha512.append(input)?
    h.assert_array_eq[U8](SHA512(input), sha512.final()?)

class \nodoc\ iso _TestDigestConcatenation is Property2[USize, USize]
  fun name(): String => "crypto/Digest/property/concatenation"

  fun gen1(): Generator[USize] =>
    Generators.usize(0, 128)

  fun gen2(): Generator[USize] =>
    Generators.usize(0, 128)

  fun ref property2(size1: USize, size2: USize, h: TestHelper) ? =>
    let part1 = recover val Array[U8].init(0xAA, size1) end
    let part2 = recover val Array[U8].init(0xBB, size2) end
    let combined =
      recover val
        Array[U8](size1 + size2)
          .> concat(part1.values())
          .> concat(part2.values())
      end

    let d = Digest.sha256()?
    d.append(part1)?
    d.append(part2)?
    h.assert_array_eq[U8](SHA256(combined), d.final()?)

class \nodoc\ iso _TestDigestOutputLength is Property[USize]
  fun name(): String => "crypto/Digest/property/output_length"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) ? =>
    let input = recover val Array[U8].init(0x42, sample) end

    let md5 = Digest.md5()?
    md5.append(input)?
    h.assert_eq[USize](md5.digest_size(), md5.final()?.size())

    let ripemd160 = Digest.ripemd160()?
    ripemd160.append(input)?
    h.assert_eq[USize](ripemd160.digest_size(), ripemd160.final()?.size())

    let sha1 = Digest.sha1()?
    sha1.append(input)?
    h.assert_eq[USize](sha1.digest_size(), sha1.final()?.size())

    let sha224 = Digest.sha224()?
    sha224.append(input)?
    h.assert_eq[USize](sha224.digest_size(), sha224.final()?.size())

    let sha256 = Digest.sha256()?
    sha256.append(input)?
    h.assert_eq[USize](sha256.digest_size(), sha256.final()?.size())

    let sha384 = Digest.sha384()?
    sha384.append(input)?
    h.assert_eq[USize](sha384.digest_size(), sha384.final()?.size())

    let sha512 = Digest.sha512()?
    sha512.append(input)?
    h.assert_eq[USize](sha512.digest_size(), sha512.final()?.size())

class \nodoc\ iso _TestConstantTimeCompareReflexive is Property[USize]
  fun name(): String =>
    "crypto/ConstantTimeCompare/property/reflexive"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) =>
    let input = recover val Array[U8].init(0x42, sample) end
    h.assert_true(ConstantTimeCompare(input, input))

class \nodoc\ iso _TestConstantTimeCompareSensitive
  is Property2[USize, USize]
  fun name(): String =>
    "crypto/ConstantTimeCompare/property/sensitive"

  fun gen1(): Generator[USize] =>
    Generators.usize(1, 256)

  fun gen2(): Generator[USize] =>
    Generators.usize(0, 255)

  fun ref property2(size: USize, hint: USize, h: TestHelper) ? =>
    let pos = hint % size
    let original = recover val Array[U8].init(0x42, size) end
    let modified =
      recover val
        let a = original.clone()
        a(pos)? = a(pos)? xor 0xFF
        a
      end
    h.assert_false(ConstantTimeCompare(original, modified))

class \nodoc\ iso _TestToHexStringLength is Property[USize]
  fun name(): String => "crypto/ToHexString/property/length"

  fun gen(): Generator[USize] =>
    Generators.usize(0, 256)

  fun ref property(sample: USize, h: TestHelper) =>
    let input = recover val Array[U8].init(0x42, sample) end
    h.assert_eq[USize](input.size() * 2, ToHexString(input).size())

class \nodoc\ iso _TestToHexStringValidHex is Property[U8]
  fun name(): String => "crypto/ToHexString/property/valid_hex"

  fun gen(): Generator[U8] =>
    Generators.u8()

  fun ref property(sample: U8, h: TestHelper) =>
    let input = recover val [sample] end
    let hex = ToHexString(input)
    for c in hex.values() do
      h.assert_true(
        ((c >= '0') and (c <= '9')) or
        ((c >= 'a') and (c <= 'f')))
    end
