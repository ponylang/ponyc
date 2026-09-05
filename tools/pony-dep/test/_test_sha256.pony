use "pony_test"
use dep = ".."

class \nodoc\ _TestSha256Empty is UnitTest
  fun name(): String => "Sha256/empty string"

  fun apply(h: TestHelper) =>
    let digest = dep.Sha256(recover val Array[U8] end)
    h.assert_eq[String](dep.Sha256.hex(digest),
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")

class \nodoc\ _TestSha256Abc is UnitTest
  fun name(): String => "Sha256/abc"

  fun apply(h: TestHelper) =>
    let digest = dep.Sha256("abc")
    h.assert_eq[String](dep.Sha256.hex(digest),
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")

class \nodoc\ _TestSha256TwoBlocks is UnitTest
  fun name(): String => "Sha256/448-bit message"

  fun apply(h: TestHelper) =>
    let digest = dep.Sha256(
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
    h.assert_eq[String](dep.Sha256.hex(digest),
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")

class \nodoc\ _TestSha256OneBlock is UnitTest
  fun name(): String => "Sha256/55-byte message (one-block boundary)"

  fun apply(h: TestHelper) =>
    let digest = dep.Sha256(
      "abcdefghijklmnopqrstuvwxyz12345678901234567890123456789")
    h.assert_eq[String](dep.Sha256.hex(digest),
      "2406ff49cbd6a01835dccc4d448265a1f57122b2bd0bc71926dfe9d9f5012a5f")

class \nodoc\ _TestSha256LongMessage is UnitTest
  fun name(): String => "Sha256/896-bit message"

  fun apply(h: TestHelper) =>
    let digest = dep.Sha256(
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
      + "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")
    h.assert_eq[String](dep.Sha256.hex(digest),
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1")

class \nodoc\ _TestSha256Hex is UnitTest
  fun name(): String => "Sha256/hex encoding"

  fun apply(h: TestHelper) =>
    let bytes: Array[U8] val = [0x00; 0xFF; 0x0A; 0xBC]
    h.assert_eq[String](dep.Sha256.hex(bytes), "00ff0abc")
