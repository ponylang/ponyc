"""
# Builtin Tests

This package contains the unit tests for the `builtin` package. These are here
so `builtin` doesn't have to depend on the `PonyTest` package.
"""

use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestAbs)
    test(_TestRotate)
    test(_TestStringRunes)
    test(_TestIntToString)
    test(_TestFloatToString)
    test(_TestStringToBool)
    test(_TestStringToFloat)
    test(_TestStringToU8)
    test(_TestStringToI8)
    test(_TestStringToIntLarge)
    test(_TestStringLstrip)
    test(_TestStringRstrip)
    test(_TestStringStrip)
    test(_TestStringStripNonAscii)
    test(_TestStringRemove)
    test(_TestStringSubstring)
    test(_TestStringCut)
    test(_TestStringTrim)
    test(_TestStringTrimInPlace)
    test(_TestStringTrimInPlaceWithAppend)
    test(_TestStringIsNullTerminated)
    test(_TestStringReplace)
    test(_TestStringSplit)
    test(_TestStringSplitBy)
    test(_TestStringAdd)
    test(_TestStringJoin)
    test(_TestStringCount)
    test(_TestStringCompare)
    test(_TestStringContains)
    test(_TestStringReadInt)
    test(_TestStringUTF32)
    test(_TestStringRFind)
    test(_TestStringFromArray)
    test(_TestStringFromIsoArray)
    test(_TestStringSpace)
    test(_TestStringRecalc)
    test(_TestStringTruncate)
    test(_TestStringChop)
    test(_TestStringUnchop)
    test(_TestStringRepeatStr)
    test(_TestStringConcatOffsetLen)
    test(_TestSpecialValuesF32)
    test(_TestSpecialValuesF64)
    test(_TestArrayAppend)
    test(_TestArrayConcat)
    test(_TestArraySlice)
    test(_TestArrayTrim)
    test(_TestArrayTrimInPlace)
    test(_TestArrayTrimInPlaceWithAppend)
    test(_TestArrayInsert)
    test(_TestArrayValuesRewind)
    test(_TestArrayFind)
    test(_TestArraySwapElements)
    test(_TestArrayChop)
    test(_TestArrayUnchop)
    test(_TestMath128)
    test(_TestRem)
    test(_TestMod)
    test(_TestFld)
    test(_TestAddc)
    test(_TestSubc)
    test(_TestMulc)
    test(_TestDivc)
    test(_TestFldc)
    test(_TestRemc)
    test(_TestModc)
    test(_TestSignedPartialArithmetic)
    test(_TestUnsignedPartialArithmetic)
    test(_TestNextPow2)
    test(_TestNumberConversionSaturation)
    test(_TestNullablePointer)
    test(_TestLambdaCapture)
    test(_TestValtrace)

class iso _TestAbs is UnitTest
  """
  Test abs function
  """
  fun name(): String => "builtin/Int.abs"

  fun apply(h: TestHelper) =>
    h.assert_eq[U8](128, I8(-128).abs())
    h.assert_eq[U8](3, I8(-3).abs())
    h.assert_eq[U8](14, I8(14).abs())
    h.assert_eq[U8](0, I8(0).abs())

    h.assert_eq[U16](128, I16(-128).abs())
    h.assert_eq[U16](32768, I16(-32768).abs())
    h.assert_eq[U16](3, I16(-3).abs())
    h.assert_eq[U16](27, I16(-27).abs())
    h.assert_eq[U16](0, I16(0).abs())

    h.assert_eq[U32](128, I32(-128).abs())
    h.assert_eq[U32](32768, I32(-32768).abs())
    h.assert_eq[U32](2147483648, I32(-2147483648).abs())
    h.assert_eq[U32](3, I32(-3).abs())
    h.assert_eq[U32](124, I32(-124).abs())
    h.assert_eq[U32](0, I32(0).abs())

    h.assert_eq[U64](128, I64(-128).abs())
    h.assert_eq[U64](32768, I64(-32768).abs())
    h.assert_eq[U64](2147483648, I64(-2147483648).abs())
    h.assert_eq[U64](128, I64(-128).abs())
    h.assert_eq[U64](3, I64(-3).abs())
    h.assert_eq[U64](124, I64(-124).abs())
    h.assert_eq[U64](0, I64(0).abs())

    h.assert_eq[U128](128, I128(-128).abs())
    h.assert_eq[U128](32768, I128(-32768).abs())
    h.assert_eq[U128](2147483648, I128(-2147483648).abs())
    h.assert_eq[U128](128, I128(-128).abs())
    h.assert_eq[U128](3, I128(-3).abs())
    h.assert_eq[U128](124, I128(-124).abs())
    h.assert_eq[U128](0, I128(0).abs())

class iso _TestRotate is UnitTest
  """
  Test rotl and rotr function
  """
  fun name(): String => "builtin/Int.Rotate"

  fun apply(h: TestHelper) =>
    h.assert_eq[U8](0x0F, U8(0x0F).rotl(0))
    h.assert_eq[U8](0x3C, U8(0x0F).rotl(2))
    h.assert_eq[U8](0x0F, U8(0x0F).rotl(8))
    h.assert_eq[U8](0xF0, U8(0x0F).rotl(12))

    h.assert_eq[U8](0x0F, U8(0x0F).rotr(0))
    h.assert_eq[U8](0xC3, U8(0x0F).rotr(2))
    h.assert_eq[U8](0x0F, U8(0x0F).rotr(8))
    h.assert_eq[U8](0xF0, U8(0x0F).rotr(12))

    h.assert_eq[U16](0x00F0, U16(0x00F0).rotl(0))
    h.assert_eq[U16](0x0F00, U16(0x00F0).rotl(4))
    h.assert_eq[U16](0x00F0, U16(0x00F0).rotl(16))
    h.assert_eq[U16](0x0F00, U16(0x00F0).rotl(20))

    h.assert_eq[U16](0x00F0, U16(0x00F0).rotr(0))
    h.assert_eq[U16](0x000F, U16(0x00F0).rotr(4))
    h.assert_eq[U16](0x00F0, U16(0x00F0).rotr(16))
    h.assert_eq[U16](0x000F, U16(0x00F0).rotr(20))

    h.assert_eq[U32](0x0F00, U32(0x0F00).rotl(0))
    h.assert_eq[U32](0xF000, U32(0x0F00).rotl(4))
    h.assert_eq[U32](0x0F00, U32(0x0F00).rotl(32))
    h.assert_eq[U32](0xF000, U32(0x0F00).rotl(36))

    h.assert_eq[U32](0x0F00, U32(0x0F00).rotr(0))
    h.assert_eq[U32](0x00F0, U32(0x0F00).rotr(4))
    h.assert_eq[U32](0x0F00, U32(0x0F00).rotr(32))
    h.assert_eq[U32](0x00F0, U32(0x0F00).rotr(36))

    h.assert_eq[U64](0x0F00, U64(0x0F00).rotl(0))
    h.assert_eq[U64](0xF000, U64(0x0F00).rotl(4))
    h.assert_eq[U64](0x0F00, U64(0x0F00).rotl(64))
    h.assert_eq[U64](0xF000, U64(0x0F00).rotl(68))

    h.assert_eq[U64](0x0F00, U64(0x0F00).rotr(0))
    h.assert_eq[U64](0x00F0, U64(0x0F00).rotr(4))
    h.assert_eq[U64](0x0F00, U64(0x0F00).rotr(64))
    h.assert_eq[U64](0x00F0, U64(0x0F00).rotr(68))

    h.assert_eq[U128](0x0F00, U128(0x0F00).rotl(0))
    h.assert_eq[U128](0xF000, U128(0x0F00).rotl(4))
    h.assert_eq[U128](0x0F00, U128(0x0F00).rotl(128))
    h.assert_eq[U128](0xF000, U128(0x0F00).rotl(132))

    h.assert_eq[U128](0x0F00, U128(0x0F00).rotr(0))
    h.assert_eq[U128](0x00F0, U128(0x0F00).rotr(4))
    h.assert_eq[U128](0x0F00, U128(0x0F00).rotr(128))
    h.assert_eq[U128](0x00F0, U128(0x0F00).rotr(132))

class iso _TestStringRunes is UnitTest
  """
  Test iterating over the unicode codepoints in a string.
  """
  fun name(): String => "builtin/String.runes"

  fun apply(h: TestHelper) =>
    let result = Array[U32]

    for c in "\u16ddx\ufb04".runes() do
      result.push(c)
    end

    h.assert_array_eq[U32]([0x16dd; 'x'; 0xfb04], result)

class iso _TestIntToString is UnitTest
  """
  Test converting integers to strings.
  """
  fun name(): String => "builtin/U32.string"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("0", U32(0).string())
    h.assert_eq[String]("3", U32(3).string())
    h.assert_eq[String]("1234", U32(1234).string())

class iso _TestFloatToString is UnitTest
  """
  Test converting floats to strings.
  """
  fun name(): String => "builtin/Float.string"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("0", F32(0).string())
    h.assert_eq[String]("-0.35", F32(-3.5e-1).string())
    h.assert_eq[String]("123.125", F32(123.125).string())
    h.assert_eq[String]("0", F64(0).string())
    h.assert_eq[String]("-0.35", F64(-3.5e-1).string())
    h.assert_eq[String]("123.125", F64(123.125).string())

class iso _TestStringToBool is UnitTest
  """
  Test converting strings to Bools.
  """
  fun name(): String => "builtin/String.bool"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[Bool](false, "false".bool()?)
    h.assert_eq[Bool](false, "FALSE".bool()?)
    h.assert_eq[Bool](true, "true".bool()?)
    h.assert_eq[Bool](true, "TRUE".bool()?)

    h.assert_error({() ? => "bogus".bool()? })

class iso _TestStringToFloat is UnitTest
  """
  Test converting strings to floats.
  """
  fun name(): String => "builtin/String.float"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[F32](4.125, "4.125".f32()?)
    h.assert_eq[F64](4.125, "4.125".f64()?)

    h.assert_eq[F32](-4.125e-3, "-4.125e-3".f32()?)
    h.assert_eq[F64](-4.125e-3, "-4.125e-3".f64()?)

    h.assert_eq[F32](1.0, "+1.".f32()?)
    h.assert_eq[F64](1.0, "1.".f64()?)

    // decimal floating-point expression
    h.assert_eq[F64](1.0, "1".f64()?)
    h.assert_eq[F32](1.0, "1".f32()?)
    h.assert_eq[F64](-1234567890.23e2, "-1234567890.23e+2".f64()?)
    h.assert_eq[F32](-1234567890.23e2, "-1234567890.23e+2".f32()?)
    h.assert_eq[F64](-1234567890.23e-2, "-1234567890.23E-2".f64()?)
    h.assert_eq[F32](-1234567890.23e-2, "-1234567890.23E-2".f32()?)
    h.assert_eq[F64](1234567890.23e2, " +1234567890.23e2".f64()?)
    h.assert_eq[F32](1234567890.23e2, " +1234567890.23e2".f32()?)

    // binary floating-point expressions
    h.assert_eq[F64](16.0, "0x10".f64()?)
    h.assert_eq[F32](16.0, "0X10".f32()?)
    h.assert_eq[F64](13.9375, "0XD.F".f64()?)
    h.assert_eq[F32](892.0, "0XD.FP+6".f32()?)
    h.assert_eq[F64](4.015625, "0X10.10p-2".f64()?)

    // not a number
    h.assert_true("NaN".f32()?.nan())
    h.assert_true("+nAn".f64()?.nan())
    h.assert_true(" -nan".f64()?.nan())
    h.assert_true("NaN(123)".f64()?.nan()) // nan-boxing ftw
    h.assert_true("NaN(123)".f32()?.nan()) // nan-boxing ftw

    // infinity
    h.assert_no_error({() ? =>
      h.assert_true("Inf".f64()?.infinite())
    }, "Inf")
    h.assert_no_error({() ? =>
      h.assert_true("\t-infinity".f32()?.infinite())
    }, "\t-infinity")
    h.assert_no_error({() ? =>
      h.assert_true("+INFINITY".f64()?.infinite())
    }, "+INFINITY")


    let invalid_float_strings: Array[String] = [
      ""
      "a"
      "1.."
      "a.1"
      "~!@#$%^&*()"
      "1.18973e+4932" // overflow
      "1.18973e-4932" // underflow
      "1.12232e+4ABC" // trailing characters
      "0x"
      "Infinityi"
      ]
    for invalid_float in invalid_float_strings.values() do
      h.assert_error({() ? => invalid_float.f32()? }, invalid_float + " did not fail for .f32()")
      h.assert_error({() ? => invalid_float.f64()? }, invalid_float + " did not fail for .f64()")
    end

class iso _TestStringToU8 is UnitTest
  """
  Test converting strings to U8s.
  """
  fun name(): String => "builtin/String.u8"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U8](0, "0".u8()?)
    h.assert_eq[U8](123, "123".u8()?)
    h.assert_eq[U8](123, "0123".u8()?)
    h.assert_eq[U8](89, "089".u8()?)

    h.assert_error({() ? => "300".u8()? }, "U8 300")
    h.assert_error({() ? => "30L".u8()? }, "U8 30L")
    h.assert_error({() ? => "-10".u8()? }, "U8 -10")

    h.assert_eq[U8](16, "0x10".u8()?)
    h.assert_eq[U8](31, "0x1F".u8()?)
    h.assert_eq[U8](31, "0x1f".u8()?)
    h.assert_eq[U8](31, "0X1F".u8()?)
    h.assert_eq[U8](2, "0b10".u8()?)
    h.assert_eq[U8](2, "0B10".u8()?)
    h.assert_eq[U8](0x8A, "0b1000_1010".u8()?)

    h.assert_error({() ? => "1F".u8()? }, "U8 1F")
    h.assert_error({() ? => "0x".u8()? }, "U8 0x")
    h.assert_error({() ? => "0b3".u8()? }, "U8 0b3")
    h.assert_error({() ? => "0d4".u8()? }, "U8 0d4")

class iso _TestStringToI8 is UnitTest
  """
  Test converting strings to I8s.
  """
  fun name(): String => "builtin/String.i8"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[I8](0, "0".i8()?)
    h.assert_eq[I8](123, "123".i8()?)
    h.assert_eq[I8](123, "0123".i8()?)
    h.assert_eq[I8](89, "089".i8()?)
    h.assert_eq[I8](-10, "-10".i8()?)

    h.assert_error({() ? => "200".i8()? }, "I8 200")
    h.assert_error({() ? => "30L".i8()? }, "I8 30L")

    h.assert_eq[I8](16, "0x10".i8()?)
    h.assert_eq[I8](31, "0x1F".i8()?)
    h.assert_eq[I8](31, "0x1f".i8()?)
    h.assert_eq[I8](31, "0X1F".i8()?)
    h.assert_eq[I8](2, "0b10".i8()?)
    h.assert_eq[I8](2, "0B10".i8()?)
    h.assert_eq[I8](0x4A, "0b100_1010".i8()?)
    h.assert_eq[I8](-0x4A, "-0b100_1010".i8()?)

    h.assert_error({() ? => "1F".i8()? }, "U8 1F")
    h.assert_error({() ? => "0x".i8()? }, "U8 0x")
    h.assert_error({() ? => "0b3".i8()? }, "U8 0b3")
    h.assert_error({() ? => "0d4".i8()? }, "U8 0d4")

class iso _TestStringToIntLarge is UnitTest
  """
  Test converting strings to I* and U* types bigger than 8 bit.
  """
  fun name(): String => "builtin/String.toint"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U16](0, "0".u16()?)
    h.assert_eq[U16](123, "123".u16()?)
    h.assert_error({() ? => "-10".u16()? }, "U16 -10")
    h.assert_error({() ? => "65536".u16()? }, "U16 65536")
    h.assert_error({() ? => "30L".u16()? }, "U16 30L")

    h.assert_eq[I16](0, "0".i16()?)
    h.assert_eq[I16](123, "123".i16()?)
    h.assert_eq[I16](-10, "-10".i16()?)
    h.assert_error({() ? => "65536".i16()? }, "I16 65536")
    h.assert_error({() ? => "30L".i16()? }, "I16 30L")

    h.assert_eq[U32](0, "0".u32()?)
    h.assert_eq[U32](123, "123".u32()?)
    h.assert_error({() ? => "-10".u32()? }, "U32 -10")
    h.assert_error({() ? => "30L".u32()? }, "U32 30L")

    h.assert_eq[I32](0, "0".i32()?)
    h.assert_eq[I32](123, "123".i32()?)
    h.assert_eq[I32](-10, "-10".i32()?)
    h.assert_error({() ? => "30L".i32()? }, "I32 30L")

    h.assert_eq[U64](0, "0".u64()?)
    h.assert_eq[U64](123, "123".u64()?)
    h.assert_error({() ? => "-10".u64()? }, "U64 -10")
    h.assert_error({() ? => "30L".u64()? }, "U64 30L")

    h.assert_eq[I64](0, "0".i64()?)
    h.assert_eq[I64](123, "123".i64()?)
    h.assert_eq[I64](-10, "-10".i64()?)
    h.assert_error({() ? => "30L".i64()? }, "I64 30L")

    h.assert_eq[U128](0, "0".u128()?)
    h.assert_eq[U128](123, "123".u128()?)
    h.assert_error({() ? => "-10".u128()? }, "U128 -10")
    h.assert_error({() ? => "30L".u128()? }, "U128 30L")

    h.assert_eq[I128](0, "0".i128()?)
    h.assert_eq[I128](123, "123".i128()?)
    h.assert_eq[I128](-10, "-10".i128()?)
    h.assert_error({() ? => "30L".i128()? }, "I128 30L")

class iso _TestStringLstrip is UnitTest
  """
  Test stripping leading characters from a string.
  """
  fun name(): String => "builtin/String.lstrip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "foobar".clone() .> lstrip("foo") end, "bar")
    h.assert_eq[String](recover "fooooooobar".clone() .> lstrip("foo") end,
      "bar")
    h.assert_eq[String](recover "   foobar".clone() .> lstrip() end, "foobar")
    h.assert_eq[String](recover "  foobar  ".clone() .> lstrip() end,
      "foobar  ")

class iso _TestStringRstrip is UnitTest
  """
  Test stripping trailing characters from a string.
  """
  fun name(): String => "builtin/String.rstrip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "foobar".clone() .> rstrip("bar") end, "foo")
    h.assert_eq[String](recover "foobaaaaaar".clone() .> rstrip("bar") end,
      "foo")
    h.assert_eq[String](recover "foobar  ".clone() .> rstrip() end, "foobar")
    h.assert_eq[String](recover "  foobar  ".clone() .> rstrip() end,
      "  foobar")

class iso _TestStringStrip is UnitTest
  """
  Test stripping leading and trailing characters from a string.
  """
  fun name(): String => "builtin/String.strip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "  foobar  ".clone() .> strip() end, "foobar")
    h.assert_eq[String](recover "barfoobar".clone() .> strip("bar") end, "foo")
    h.assert_eq[String](recover "foobarfoo".clone() .> strip("foo") end, "bar")
    h.assert_eq[String](recover "foobarfoo".clone() .> strip("bar") end,
      "foobarfoo")

class iso _TestStringStripNonAscii is UnitTest
  """
  Test stripping leading and trailing characters from a string that
  contains non-ascii text data.
  """
  fun name(): String => "builtin/String.strip_non_ascii"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover " 🐎\n".clone() .> strip() end, "🐎")
    h.assert_eq[String](recover "\n🐎\n".clone() .> strip("\n") end, "🐎")
    h.assert_eq[String](recover " 🐎".clone() .> strip("🐎") end, " ")

    h.assert_eq[String](recover " ポニー ".clone() .> strip() end, "ポニー")
    h.assert_eq[String](recover "ポニー ".clone() .> strip(" ") end, "ポニー")

    h.assert_eq[String](recover " źrebię\t".clone() .> strip() end, "źrebię")
    h.assert_eq[String](recover "\tźrebię\t".clone() .> strip("\t") end, "źrebię")

    h.assert_eq[String](recover " პონი\n".clone() .> strip() end, "პონი")
    h.assert_eq[String](recover "\nპონი\n".clone() .> strip("\n") end, "პონი")


class iso _TestStringRemove is UnitTest
  """
  Test removing characters from a string (independent of leading or trailing).
  """
  fun name(): String => "builtin/String.remove"

  fun apply(h: TestHelper) =>
    let s1 = recover "  foo   bar  ".clone() end
    let s2 = recover "barfoobar".clone() end
    let s3 = recover "f-o-o-b-a-r!".clone() end
    let s4 = recover "f-o-o-b-a-r!".clone() end

    let r1 = s1.remove(" ")
    let r2 = s2.remove("foo")
    let r3 = s3.remove("-")
    let r4 = s4.remove("-!")

    h.assert_eq[USize](r1, 7)
    h.assert_eq[USize](r2, 1)
    h.assert_eq[USize](r3, 5)
    h.assert_eq[USize](r4, 0)

    h.assert_eq[String](consume s1, "foobar")
    h.assert_eq[String](consume s2, "barbar")
    h.assert_eq[String](consume s3, "foobar!")
    h.assert_eq[String](consume s4, "f-o-o-b-a-r!")

class iso _TestStringSubstring is UnitTest
  """
  Test copying range of characters.
  """
  fun name(): String => "builtin/String.substring"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("3456", "0123456".substring(3, 99))

    h.assert_eq[String]("345", "0123456".substring(3, 6))
    h.assert_eq[String]("3456", "0123456".substring(3, 7))
    h.assert_eq[String]("3456", "0123456".substring(3))
    h.assert_eq[String]("345", "0123456".substring(3, -1))

class iso _TestStringCut is UnitTest
  """
  Test cutting part of a string
  """
  fun name(): String => "builtin/String.cut"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("01236", "0123456".cut(4, 6))
    h.assert_eq[String]("0123", "0123456".cut(4, 7))
    h.assert_eq[String]("0123", "0123456".cut(4))

class iso _TestStringTrim is UnitTest
  """
  Test trimming part of a string.
  """
  fun name(): String => "builtin/String.trim"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("45", "0123456".trim(4, 6))
    h.assert_eq[USize](2, "0123456".trim(4, 6).space())
    h.assert_eq[String]("456", "0123456".trim(4, 7))
    h.assert_eq[USize](3, "0123456".trim(4, 7).space())
    h.assert_eq[String]("456", "0123456".trim(4))
    h.assert_eq[USize](3, "0123456".trim(4).space())
    h.assert_eq[String]("", "0123456".trim(4, 4))
    h.assert_eq[USize](0, "0123456".trim(4, 4).space())
    h.assert_eq[String]("", "0123456".trim(4, 1))
    h.assert_eq[USize](0, "0123456".trim(4, 1).space())
    h.assert_eq[String]("456", "0123456789".trim(1, 8).trim(3, 6))
    h.assert_eq[USize](3, "0123456789".trim(1, 8).trim(3, 6).space())
    h.assert_eq[String]("456",
      "0123456789".clone().>trim_in_place(1, 8).trim(3, 6))
    h.assert_eq[USize](3,
      "0123456789".clone().>trim_in_place(1, 8).trim(3, 6).space())

class iso _TestStringTrimInPlace is UnitTest
  """
  Test trimming part of a string in place.
  """
  fun name(): String => "builtin/String.trim_in_place"

  fun apply(h: TestHelper) =>
    case(h, "45", "0123456", 4, 6, 2)
    case(h, "456", "0123456", 4, 7, 3)
    case(h, "456", "0123456", 4 where space = 3)
    case(h, "", "0123456", 4, 4, 0)
    case(h, "", "0123456", 4, 1, 0)
    case(h, "456", "0123456789".clone().>trim_in_place(1, 8), 3, 6, 3)
    case(h, "456", "0123456789".trim(1, 8), 3, 6, 3)
    case(h, "", "0123456789".clone().>trim_in_place(1, 8), 3, 3, 0)

  fun case(
    h: TestHelper,
    expected: String,
    orig: String,
    from: USize,
    to: USize = -1,
    space: USize = 0)
  =>
    let copy: String ref = orig.clone()
    let pre_trim_pagemap = @ponyint_pagemap_get[Pointer[None]](copy.cpointer())
    copy.trim_in_place(from, to)
    h.assert_eq[String box](expected, copy)
    h.assert_eq[USize](space, copy.space())
    h.assert_eq[String box](expected, copy.clone()) // safe to clone
    let post_trim_pagemap = @ponyint_pagemap_get[Pointer[None]](copy.cpointer())
    if copy.space() == 0 then
      h.assert_eq[USize](0, post_trim_pagemap.usize())
    else
      h.assert_eq[USize](pre_trim_pagemap.usize(), post_trim_pagemap.usize())
    end

class iso _TestStringTrimInPlaceWithAppend is UnitTest
  """
  Test trimming part of a string in place then append and trim again

  Verifies we don't get a regression for:
  https://github.com/ponylang/ponyc/issues/1996
  """
  fun name(): String => "builtin/String.trim_in_place_with_append"

  fun apply(h: TestHelper) =>
    let a: String ref = "Hello".clone()
    let big: Array[U8] val = recover val Array[U8].init(U8(1), 12_000) end
    a.trim_in_place(a.size())
    h.assert_eq[String box]("", a)
    a.append(big)
    a.trim_in_place(a.size())
    h.assert_eq[String box]("", a)
    a.append("Hello")
    h.assert_eq[String box]("Hello", a)

class iso _TestStringIsNullTerminated is UnitTest
  """
  Test checking if a string is null terminated.
  """
  fun name(): String => "builtin/String.is_null_terminated"

  fun apply(h: TestHelper) =>
    h.assert_true("".is_null_terminated())
    h.assert_true("0123456".is_null_terminated())
    h.assert_true("0123456".trim(4, 7).is_null_terminated())
    h.assert_false("0123456".trim(2, 4).is_null_terminated())
    h.assert_true("0123456".trim(2, 4).clone().is_null_terminated())
    h.assert_false("0123456".trim(2, 4).is_null_terminated())

    h.assert_true(
      String.from_iso_array(recover
        ['a'; 'b'; 'c']
      end).is_null_terminated())
    h.assert_false(
      String.from_iso_array(recover
        ['a'; 'b'; 'c'; 'd'; 'e'; 'f'; 'g'; 'h'] // power of two sized array
      end).is_null_terminated())

class iso _TestSpecialValuesF32 is UnitTest
  """
  Test whether a F32 is infinite or NaN.
  """
  fun name(): String => "builtin/F32.finite"

  fun apply(h: TestHelper) =>
    // 1
    h.assert_true(F32(1.0).finite())
    h.assert_false(F32(1.0).infinite())
    h.assert_false(F32(1.0).nan())

    // -1
    h.assert_true(F32(-1.0).finite())
    h.assert_false(F32(-1.0).infinite())
    h.assert_false(F32(-1.0).nan())

    // Infinity
    h.assert_false(F32(1.0 / 0.0).finite())
    h.assert_true(F32(1.0 / 0.0).infinite())
    h.assert_false(F32(1.0 / 0.0).nan())

    // - infinity
    h.assert_false(F32(-1.0 / 0.0).finite())
    h.assert_true(F32(-1.0 / 0.0).infinite())
    h.assert_false(F32(-1.0 / 0.0).nan())

    // NaN
    h.assert_false(F32(0.0 / 0.0).finite())
    h.assert_false(F32(0.0 / 0.0).infinite())
    h.assert_true(F32(0.0 / 0.0).nan())

class iso _TestSpecialValuesF64 is UnitTest
  """
  Test whether a F64 is infinite or NaN.
  """
  fun name(): String => "builtin/F64.finite"

  fun apply(h: TestHelper) =>
    // 1
    h.assert_true(F64(1.0).finite())
    h.assert_false(F64(1.0).infinite())
    h.assert_false(F64(1.0).nan())

    // -1
    h.assert_true(F64(-1.0).finite())
    h.assert_false(F64(-1.0).infinite())
    h.assert_false(F64(-1.0).nan())

    // Infinity
    h.assert_false(F64(1.0 / 0.0).finite())
    h.assert_true(F64(1.0 / 0.0).infinite())
    h.assert_false(F64(1.0 / 0.0).nan())

    // - infinity
    h.assert_false(F64(-1.0 / 0.0).finite())
    h.assert_true(F64(-1.0 / 0.0).infinite())
    h.assert_false(F64(-1.0 / 0.0).nan())

    // NaN
    h.assert_false(F64(0.0 / 0.0).finite())
    h.assert_false(F64(0.0 / 0.0).infinite())
    h.assert_true(F64(0.0 / 0.0).nan())

class iso _TestStringReplace is UnitTest
  """
  Test String.replace
  """
  fun name(): String => "builtin/String.replace"

  fun apply(h: TestHelper) =>
    let s = String .> append("this is a robbery, this is a stickup")
    s.replace("is a", "is not a")
    h.assert_eq[String box](s, "this is not a robbery, this is not a stickup")

class iso _TestStringSplit is UnitTest
  """
  Test String.split
  """
  fun name(): String => "builtin/String.split"

  fun apply(h: TestHelper) ? =>
    var r = "1 2 3  4".split()
    h.assert_eq[USize](r.size(), 5)
    h.assert_eq[String](r(0)?, "1")
    h.assert_eq[String](r(1)?, "2")
    h.assert_eq[String](r(2)?, "3")
    h.assert_eq[String](r(3)?, "")
    h.assert_eq[String](r(4)?, "4")

    r = "1.2,.3,, 4".split(".,")
    h.assert_eq[USize](r.size(), 6)
    h.assert_eq[String](r(0)?, "1")
    h.assert_eq[String](r(1)?, "2")
    h.assert_eq[String](r(2)?, "")
    h.assert_eq[String](r(3)?, "3")
    h.assert_eq[String](r(4)?, "")
    h.assert_eq[String](r(5)?, " 4")

    r = "1 2 3  4".split(where n = 3)
    h.assert_eq[USize](r.size(), 3)
    h.assert_eq[String](r(0)?, "1")
    h.assert_eq[String](r(1)?, "2")
    h.assert_eq[String](r(2)?, "3  4")

    r = "1.2,.3,, 4".split(".,", 4)
    h.assert_eq[USize](r.size(), 4)
    h.assert_eq[String](r(0)?, "1")
    h.assert_eq[String](r(1)?, "2")
    h.assert_eq[String](r(2)?, "")
    h.assert_eq[String](r(3)?, "3,, 4")

class iso _TestStringSplitBy is UnitTest
  """
  Test String.split_by
  """
  fun name(): String => "builtin/String.split_by"

  fun apply(h: TestHelper) ? =>
    var r = "opinion".split_by("pi")
    h.assert_eq[USize](r.size(), 2)
    h.assert_eq[String](r(0)?, "o")
    h.assert_eq[String](r(1)?, "nion")

    r = "opopgadget".split_by("op")
    h.assert_eq[USize](r.size(), 3)
    h.assert_eq[String](r(0)?, "")
    h.assert_eq[String](r(1)?, "")
    h.assert_eq[String](r(2)?, "gadget")

    r = "simple spaces, with one trailing ".split_by(" ")
    h.assert_eq[USize](r.size(), 6)
    h.assert_eq[String](r(0)?, "simple")
    h.assert_eq[String](r(1)?, "spaces,")
    h.assert_eq[String](r(2)?, "with")
    h.assert_eq[String](r(3)?, "one")
    h.assert_eq[String](r(4)?, "trailing")
    h.assert_eq[String](r(5)?, "")

    r = " with more trailing  ".split_by(" ")
    h.assert_eq[USize](r.size(), 6)
    h.assert_eq[String](r(0)?, "")
    h.assert_eq[String](r(1)?, "with")
    h.assert_eq[String](r(2)?, "more")
    h.assert_eq[String](r(3)?, "trailing")
    h.assert_eq[String](r(4)?, "")
    h.assert_eq[String](r(5)?, "")

    r = "should not split this too much".split(" ", 3)
    h.assert_eq[USize](r.size(), 3)
    h.assert_eq[String](r(0)?, "should")
    h.assert_eq[String](r(1)?, "not")
    h.assert_eq[String](r(2)?, "split this too much")

    let s = "this should not even be split"
    r = s.split_by(" ", 0)
    h.assert_eq[USize](r.size(), 1)
    h.assert_eq[String](r(0)?, s)

    r = s.split_by("")
    h.assert_eq[USize](r.size(), 1)
    h.assert_eq[String](r(0)?, s)

    r = "make some ☃s and ☺ for the winter ☺".split_by("☃")
    h.assert_eq[USize](r.size(), 2)
    h.assert_eq[String](r(0)?, "make some ")
    h.assert_eq[String](r(1)?, "s and ☺ for the winter ☺")

    r = "try with trailing patternpatternpattern".split_by("pattern", 2)
    h.assert_eq[USize](r.size(), 2)
    h.assert_eq[String](r(0)?, "try with trailing ")
    h.assert_eq[String](r(1)?, "patternpattern")

class iso _TestStringAdd is UnitTest
  """
  Test String.add
  """
  fun name(): String => "builtin/String.add"

  fun apply(h: TestHelper) =>
    let empty = String.from_array(recover Array[U8] end)
    h.assert_eq[String]("a" + "b", "ab")
    h.assert_eq[String](empty + "a", "a")
    h.assert_eq[String]("a" + empty, "a")
    h.assert_eq[String](empty + empty, "")
    h.assert_eq[String](empty + "", "")
    h.assert_eq[String]("" + empty, "")
    h.assert_eq[String]("a" + "abc".trim(1, 2), "ab")
    h.assert_eq[String]("" + "abc".trim(1, 2), "b")
    h.assert_eq[String]("a" + "".trim(1, 1), "a")

class iso _TestStringJoin is UnitTest
  """
  Test String.join
  """
  fun name(): String => "builtin/String.join"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("_".join(["zomg"].values()), "zomg")
    h.assert_eq[String]("_".join(["hi"; "there"].values()), "hi_there")
    h.assert_eq[String](" ".join(["1"; ""; "2"; ""].values()), "1  2 ")
    h.assert_eq[String](" ".join([U32(1); U32(4)].values()), "1 4")
    h.assert_eq[String](" ".join(Array[String].values()), "")

class iso _TestStringCount is UnitTest
  """
  Test String.count
  """
  fun name(): String => "builtin/String.count"

  fun apply(h: TestHelper) =>
    let testString: String = "testString"
    h.assert_eq[USize](testString.count(testString), 1)
    h.assert_eq[USize](testString.count("testString"), 1)
    h.assert_eq[USize]("testString".count(testString), 1)
    h.assert_eq[USize]("".count("zomg"), 0)
    h.assert_eq[USize]("zomg".count(""), 0)
    h.assert_eq[USize]("azomg".count("zomg"), 1)
    h.assert_eq[USize]("zomga".count("zomg"), 1)
    h.assert_eq[USize]("atatat".count("at"), 3)
    h.assert_eq[USize]("atatbat".count("at"), 3)
    h.assert_eq[USize]("atata".count("ata"), 1)
    h.assert_eq[USize]("tttt".count("tt"), 2)

class iso _TestStringCompare is UnitTest
  """
  Test comparing strings.
  """
  fun name(): String => "builtin/String.compare"

  fun apply(h: TestHelper) =>
    h.assert_eq[Compare](Equal, "foo".compare("foo"))
    h.assert_eq[Compare](Greater, "foo".compare("bar"))
    h.assert_eq[Compare](Less, "bar".compare("foo"))

    h.assert_eq[Compare](Less, "abc".compare("bc"))

    h.assert_eq[Compare](Equal, "foo".compare_sub("foo", 3))
    h.assert_eq[Compare](Greater, "foo".compare_sub("bar", 3))
    h.assert_eq[Compare](Less, "bar".compare_sub("foo", 3))

    h.assert_eq[Compare](Equal, "foobar".compare_sub("foo", 3))
    h.assert_eq[Compare](Greater, "foobar".compare_sub("foo", 4))

    h.assert_eq[Compare](Less, "foobar".compare_sub("oob", 3, 0))
    h.assert_eq[Compare](Equal, "foobar".compare_sub("oob", 3, 1))

    h.assert_eq[Compare](
      Equal, "ab".compare_sub("ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("Ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("aB", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("AB", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Less, "AB".compare_sub("ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("AB", 2 where ignore_case = false))

    h.assert_eq[Compare](
      Equal, "ab".compare_sub("ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("Ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("aB", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("AB", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("AB", 2 where ignore_case = true))

    h.assert_eq[Compare](Equal, "foobar".compare_sub("bar", 2, -2, -2))

    h.assert_eq[Compare](Greater, "one".compare("four"),
      "\"one\" > \"four\"")
    h.assert_eq[Compare](Less, "four".compare("one"),
      "\"four\" < \"one\"")
    h.assert_eq[Compare](Equal, "one".compare("one"),
      "\"one\" == \"one\"")
    h.assert_eq[Compare](Less, "abc".compare("abcd"),
      "\"abc\" < \"abcd\"")
    h.assert_eq[Compare](Greater, "abcd".compare("abc"),
      "\"abcd\" > \"abc\"")
    h.assert_eq[Compare](Equal, "abcd".compare_sub("abc", 3),
      "\"abcx\" == \"abc\"")
    h.assert_eq[Compare](Equal, "abc".compare_sub("abcd", 3),
      "\"abc\" == \"abcx\"")
    h.assert_eq[Compare](Equal, "abc".compare_sub("abc", 4),
      "\"abc\" == \"abc\"")
    h.assert_eq[Compare](Equal, "abc".compare_sub("babc", 4, 1, 2),
      "\"xbc\" == \"xxbc\"")

class iso _TestStringContains is UnitTest
  fun name(): String => "builtin/String.contains"

  fun apply(h: TestHelper) =>
    let s = "hello there"

    h.assert_eq[Bool](s.contains("h"), true)
    h.assert_eq[Bool](s.contains("e"), true)
    h.assert_eq[Bool](s.contains("l"), true)
    h.assert_eq[Bool](s.contains("l"), true)
    h.assert_eq[Bool](s.contains("o"), true)
    h.assert_eq[Bool](s.contains(" "), true)
    h.assert_eq[Bool](s.contains("t"), true)
    h.assert_eq[Bool](s.contains("h"), true)
    h.assert_eq[Bool](s.contains("e"), true)
    h.assert_eq[Bool](s.contains("r"), true)
    h.assert_eq[Bool](s.contains("e"), true)

    h.assert_eq[Bool](s.contains("hel"), true)
    h.assert_eq[Bool](s.contains("o th"), true)
    h.assert_eq[Bool](s.contains("er"), true)

    h.assert_eq[Bool](s.contains("a"), false)
    h.assert_eq[Bool](s.contains("b"), false)
    h.assert_eq[Bool](s.contains("c"), false)
    h.assert_eq[Bool](s.contains("d"), false)
    h.assert_eq[Bool](s.contains("!"), false)
    h.assert_eq[Bool](s.contains("?"), false)

    h.assert_eq[Bool](s.contains("h th"), false)
    h.assert_eq[Bool](s.contains("ere "), false)
    h.assert_eq[Bool](s.contains(" hello"), false)
    h.assert_eq[Bool](s.contains("?!"), false)

class iso _TestStringReadInt is UnitTest
  """
  Test converting string at given index to integer.
  """
  fun name(): String => "builtin/String.read_int"

  fun apply(h: TestHelper) ? =>
    // 8-bit
    let u8_lo = "...0...".read_int[U8](3, 10)?
    let u8_hi = "...255...".read_int[U8](3, 10)?
    let i8_lo = "...-128...".read_int[I8](3, 10)?
    let i8_hi = "...127...".read_int[I8](3, 10)?

    h.assert_true((u8_lo._1 == 0) and (u8_lo._2 == 1))
    h.assert_true((u8_hi._1 == 255) and (u8_hi._2 == 3))
    h.assert_true((i8_lo._1 == -128) and (i8_lo._2 == 4))
    h.assert_true((i8_hi._1 == 127) and (i8_hi._2 == 3))

    // 32-bit
    let u32_lo = "...0...".read_int[U32](3, 10)?
    let u32_hi = "...4_294_967_295...".read_int[U32](3, 10)?
    let i32_lo = "...-2147483648...".read_int[I32](3, 10)?
    let i32_hi = "...2147483647...".read_int[I32](3, 10)?

    h.assert_true((u32_lo._1 == 0) and (u32_lo._2 == 1))
    h.assert_true((u32_hi._1 == 4_294_967_295) and (u32_hi._2 == 13))
    h.assert_true((i32_lo._1 == -2147483648) and (i32_lo._2 == 11))
    h.assert_true((i32_hi._1 == 2147483647) and (i32_hi._2 == 10))

    // hexadecimal
    let hexa = "...DEADBEEF...".read_int[U32](3, 16)?
    h.assert_true((hexa._1 == 0xDEADBEEF) and (hexa._2 == 8))

    // octal
    let oct = "...777...".read_int[U16](3, 8)?
    h.assert_true((oct._1 == 511) and (oct._2 == 3))

    // misc
    var u8_misc = "...000...".read_int[U8](3, 10)?
    h.assert_true((u8_misc._1 == 0) and (u8_misc._2 == 3))

    u8_misc = "...-123...".read_int[U8](3, 10)?
    h.assert_true((u8_misc._1 == 0) and (u8_misc._2 == 0))

    u8_misc = "...-0...".read_int[U8](3, 10)?
    h.assert_true((u8_misc._1 == 0) and (u8_misc._2 == 0))

class iso _TestStringUTF32 is UnitTest
  """
  Test the UTF32 encoding and decoding
  """
  fun name(): String => "builtin/String.utf32"

  fun apply(h: TestHelper) ? =>
    var s = String.from_utf32(' ')
    h.assert_eq[USize](1, s.size())
    h.assert_eq[U8](' ', s(0)?)
    h.assert_eq[U32](' ', s.utf32(0)?._1)

    s.push_utf32('\n')
    h.assert_eq[USize](2, s.size())
    h.assert_eq[U8]('\n', s(1)?)
    h.assert_eq[U32]('\n', s.utf32(1)?._1)

    s = String.create()
    s.push_utf32(0xA9) // (c)
    h.assert_eq[USize](2, s.size())
    h.assert_eq[U8](0xC2, s(0)?)
    h.assert_eq[U8](0xA9, s(1)?)
    h.assert_eq[U32](0xA9, s.utf32(0)?._1)

    s = String.create()
    s.push_utf32(0x4E0C) // a CJK Unified Ideographs which looks like Pi
    h.assert_eq[USize](3, s.size())
    h.assert_eq[U8](0xE4, s(0)?)
    h.assert_eq[U8](0xB8, s(1)?)
    h.assert_eq[U8](0x8C, s(2)?)
    h.assert_eq[U32](0x4E0C, s.utf32(0)?._1)

    s = String.create()
    s.push_utf32(0x2070E) // first character found there: http://www.i18nguy.com/unicode/supplementary-test.html
    h.assert_eq[USize](4, s.size())
    h.assert_eq[U8](0xF0, s(0)?)
    h.assert_eq[U8](0xA0, s(1)?)
    h.assert_eq[U8](0x9C, s(2)?)
    h.assert_eq[U8](0x8E, s(3)?)
    h.assert_eq[U32](0x2070E, s.utf32(0)?._1)

class iso _TestStringRFind is UnitTest
  fun name(): String => "builtin/String.rfind"

  fun apply(h: TestHelper) ? =>
    let s = "-foo-bar-baz-"
    h.assert_eq[ISize](s.rfind("-")?, 12)
    h.assert_eq[ISize](s.rfind("-", -2)?, 8)
    h.assert_eq[ISize](s.rfind("-bar", 7)?, 4)

class iso _TestStringFromArray is UnitTest
  fun name(): String => "builtin/String.from_array"

  fun apply(h: TestHelper) =>
    let s_null = String.from_array(recover ['f'; 'o'; 'o'; 0] end)
    h.assert_eq[String](s_null, "foo\x00")
    h.assert_eq[USize](s_null.size(), 4)

    let s_no_null = String.from_array(recover ['f'; 'o'; 'o'] end)
    h.assert_eq[String](s_no_null, "foo")
    h.assert_eq[USize](s_no_null.size(), 3)

class iso _TestStringFromIsoArray is UnitTest
  fun name(): String => "builtin/String.from_iso_array"

  fun apply(h: TestHelper) =>
    let s = recover val String.from_iso_array(recover ['f'; 'o'; 'o'] end) end
    h.assert_eq[String](s, "foo")
    h.assert_eq[USize](s.size(), 3)
    h.assert_true((s.space() == 3) xor s.is_null_terminated())

    let s2 =
      recover val
        String.from_iso_array(recover
          ['1'; '1'; '1'; '1'; '1'; '1'; '1'; '1']
        end)
      end
    h.assert_eq[String](s2, "11111111")
    h.assert_eq[USize](s2.size(), 8)
    h.assert_true((s2.space() == 8) xor s2.is_null_terminated())

class iso _TestStringSpace is UnitTest
  fun name(): String => "builtin/String.space"

  fun apply(h: TestHelper) =>
    let s =
      String.from_iso_array(recover
        ['1'; '1'; '1'; '1'; '1'; '1'; '1'; '1']
      end)

    h.assert_eq[USize](s.size(), 8)
    h.assert_eq[USize](s.space(), 8)
    h.assert_false(s.is_null_terminated())

class iso _TestStringRecalc is UnitTest
  fun name(): String => "builtin/String.recalc"

  fun apply(h: TestHelper) =>
    let s: String ref =
      String.from_iso_array(recover
        ['1'; '1'; '1'; '1'; '1'; '1'; '1'; '1']
      end)
    s.recalc()
    h.assert_eq[USize](s.size(), 8)
    h.assert_eq[USize](s.space(), 8)
    h.assert_false(s.is_null_terminated())

    let s2: String ref = "foobar".clone()
    s2.recalc()
    h.assert_eq[USize](s2.size(), 6)
    h.assert_eq[USize](s2.space(), 6)
    h.assert_true(s2.is_null_terminated())

    let s3: String ref =
      String.from_iso_array(recover ['1'; 0; 0; 0; 0; 0; 0; '1'] end)
    s3.truncate(1)
    s3.recalc()
    h.assert_eq[USize](s3.size(), 1)
    h.assert_eq[USize](s3.space(), 7)
    h.assert_true(s3.is_null_terminated())

class iso _TestStringTruncate is UnitTest
  fun name(): String => "builtin/String.truncate"

  fun apply(h: TestHelper) =>
    let s =
      recover ref
        String.from_iso_array(recover
          ['1'; '1'; '1'; '1'; '1'; '1'; '1'; '1']
        end)
      end
    s.truncate(s.space())
    h.assert_true(s.is_null_terminated())
    h.assert_eq[String](s.clone(), "11111111")
    h.assert_eq[USize](s.size(), 8)
    h.assert_eq[USize](s.space(), 15) // created extra allocation for null

    s.truncate(100)
    h.assert_true(s.is_null_terminated())
    h.assert_eq[USize](s.size(), 16) // sized up to _alloc
    h.assert_eq[USize](s.space(), 31) // created extra allocation for null

    s.truncate(3)
    h.assert_true(s.is_null_terminated())
    h.assert_eq[String](s.clone(), "111")
    h.assert_eq[USize](s.size(), 3)
    h.assert_eq[USize](s.space(), 31)

class iso _TestStringChop is UnitTest
  """
  Test chopping a string
  """
  fun name(): String => "builtin/String.chop"

  fun apply(h: TestHelper) =>
    case(h, "0123", "456", "0123456".clone(), 4)
    case(h, "012345", "6", "0123456".clone(), 6)
    case(h, "0", "123456", "0123456".clone(), 1)
    case(h, "0123456", "", "0123456".clone(), 7)
    case(h, "", "0123456", "0123456".clone(), 0)
    case(h, "0123", "456", "0123456789".clone().chop(7)._1, 4)
    case(h, "0123456", "", "0123456".clone(), 10)

  fun case(
    h: TestHelper,
    expected_left: String,
    expected_right: String,
    orig: String iso,
    split_point: USize)
  =>
    (let left: String iso, let right: String iso) = (consume orig).chop(split_point)
    h.assert_eq[String box](expected_left, consume left)
    h.assert_eq[String box](expected_right, consume right)

class iso _TestStringUnchop is UnitTest
  """
  Test unchopping a string
  """
  fun name(): String => "builtin/String.unchop"

  fun apply(h: TestHelper) =>
    case(h, "0123456", "0123456".clone(), 4, true)
    case(h, "0123456", "0123456".clone(), 6, true)
    case(h, "0123456", "0123456".clone(), 1, true)
    case(h, "0123456", "0123456".clone(), 7, true)
    case(h, "0123456", "0123456".clone(), 0, false)
    case(h, "0123456", "0123456".clone(), 10, true)
    let s: String = "0123456789" * 20
    case(h, s, s.clone(), None, true)

  fun case(
    h: TestHelper,
    orig: String,
    orig_clone1: String iso,
    split_point: (USize | None),
    is_true: Bool)
  =>
    let str_tag: String tag = orig_clone1
    try
      let unchopped = chop_unchop(consume orig_clone1, split_point)?
      if is_true then
        h.assert_is[String tag](str_tag, unchopped)
      else
        h.assert_isnt[String tag](str_tag, unchopped)
      end
      h.assert_eq[String box](orig, consume unchopped)
    else
      h.assert_eq[String box](orig, "")
    end

  fun chop_unchop(x: String iso, split_point: (USize | None)): String iso^ ? =>
    if x.size() < 4 then
      return consume x
    end

    let sp = match split_point
             | None => x.size()/2
             | let z: USize => z
             end
    (let left: String iso, let right: String iso) = (consume x).chop(sp)
    let unchopped = (chop_unchop(consume left, None)?)
      .unchop(chop_unchop(consume right, None)?)

    match consume unchopped
    | let y: String iso =>
      (let left2: String iso, let right2: String iso) = (consume y).chop(sp)
      let unchopped2 = (chop_unchop(consume left2, None)?)
        .unchop(chop_unchop(consume right2, None)?)

      match consume unchopped2
      | let z: String iso => return consume z
      else
        error
      end
    else
      error
    end

class iso _TestStringRepeatStr is UnitTest
  """
  Test repeating a string
  """
  fun name(): String => "builtin/String.repeat_str"

  fun apply(h: TestHelper) =>
    h.assert_eq[String box]("", "123".repeat_str(0))
    h.assert_eq[String box]("123", "123".repeat_str(1))
    h.assert_eq[String box]("123123", "123".repeat_str(2))
    h.assert_eq[String box]("123123123", "123".repeat_str(3))
    h.assert_eq[String box]("123123123123", "123".repeat_str(4))
    h.assert_eq[String box]("123123123123123", "123".repeat_str(5))
    h.assert_eq[String box]("123123123123123123", "123".repeat_str(6))
    h.assert_eq[String box]("123, 123, 123, 123, 123, 123",
      "123".repeat_str(6, ", "))
    h.assert_eq[String box]("123123123123123123", "123" * 6)

class iso _TestStringConcatOffsetLen is UnitTest
  """
  Test String.concat working correctly for non-default offset and len arguments
  """
  fun name(): String => "builtin/String.concat"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover String.>concat("ABCD".values(), 1, 2) end, "BC")

class iso _TestArrayAppend is UnitTest
  fun name(): String => "builtin/Array.append"

  fun apply(h: TestHelper) ? =>
    var a = ["one"; "two"; "three"]
    var b = ["four"; "five"; "six"]
    a.append(b)
    h.assert_eq[USize](a.size(), 6)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("four", a(3)?)
    h.assert_eq[String]("five", a(4)?)
    h.assert_eq[String]("six", a(5)?)

    a = ["one"; "two"; "three"]
    b = ["four"; "five"; "six"]
    a.append(b, 1)
    h.assert_eq[USize](a.size(), 5)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("five", a(3)?)
    h.assert_eq[String]("six", a(4)?)

    a = ["one"; "two"; "three"]
    b = ["four"; "five"; "six"]
    a.append(b, 1, 1)
    h.assert_eq[USize](a.size(), 4)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("five", a(3)?)

class iso _TestArrayConcat is UnitTest
  fun name(): String => "builtin/Array.concat"

  fun apply(h: TestHelper) ? =>
    var a = ["one"; "two"; "three"]
    var b = ["four"; "five"; "six"]
    a.concat(b.values())
    h.assert_eq[USize](a.size(), 6)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("four", a(3)?)
    h.assert_eq[String]("five", a(4)?)
    h.assert_eq[String]("six", a(5)?)

    a = ["one"; "two"; "three"]
    b = ["four"; "five"; "six"]
    a.concat(b.values(), 1)
    h.assert_eq[USize](a.size(), 5)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("five", a(3)?)
    h.assert_eq[String]("six", a(4)?)

    a = ["one"; "two"; "three"]
    b = ["four"; "five"; "six"]
    a.concat(b.values(), 1, 1)
    h.assert_eq[USize](a.size(), 4)
    h.assert_eq[String]("one", a(0)?)
    h.assert_eq[String]("two", a(1)?)
    h.assert_eq[String]("three", a(2)?)
    h.assert_eq[String]("five", a(3)?)

class iso _TestArraySlice is UnitTest
  """
  Test slicing arrays.
  """
  fun name(): String => "builtin/Array.slice"

  fun apply(h: TestHelper) ? =>
    let a = ["one"; "two"; "three"; "four"; "five"]

    let b = a.slice(1, 4)
    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[String]("two", b(0)?)
    h.assert_eq[String]("three", b(1)?)
    h.assert_eq[String]("four", b(2)?)

    let c = a.slice(0, 5, 2)
    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[String]("one", c(0)?)
    h.assert_eq[String]("three", c(1)?)
    h.assert_eq[String]("five", c(2)?)

    let d = a.reverse()
    h.assert_eq[USize](d.size(), 5)
    h.assert_eq[String]("five", d(0)?)
    h.assert_eq[String]("four", d(1)?)
    h.assert_eq[String]("three", d(2)?)
    h.assert_eq[String]("two", d(3)?)
    h.assert_eq[String]("one", d(4)?)

    let e = a.permute(Reverse(4, 0, 2))?
    h.assert_eq[USize](e.size(), 3)
    h.assert_eq[String]("five", e(0)?)
    h.assert_eq[String]("three", e(1)?)
    h.assert_eq[String]("one", e(2)?)

class iso _TestArrayTrim is UnitTest
  """
  Test trimming part of a string.
  """
  fun name(): String => "builtin/Array.trim"

  fun apply(h: TestHelper) =>
    let orig: Array[U8] val = [0; 1; 2; 3; 4; 5; 6]
    h.assert_array_eq[U8]([4; 5], orig.trim(4, 6))
    h.assert_eq[USize](2, orig.trim(4, 6).space())
    h.assert_array_eq[U8]([4; 5; 6], orig.trim(4, 7))
    h.assert_eq[USize](4, orig.trim(4, 7).space())
    h.assert_array_eq[U8]([4; 5; 6], orig.trim(4))
    h.assert_eq[USize](4, orig.trim(4).space())
    h.assert_array_eq[U8](Array[U8], orig.trim(4, 4))
    h.assert_eq[USize](0, orig.trim(4, 4).space())
    h.assert_array_eq[U8](Array[U8], orig.trim(4, 1))
    h.assert_eq[USize](0, orig.trim(4, 1).space())

class iso _TestArrayTrimInPlace is UnitTest
  """
  Test trimming part of a string in place.
  """
  fun name(): String => "builtin/Array.trim_in_place"

  fun apply(h: TestHelper) =>
    case(h, [4; 5], [0; 1; 2; 3; 4; 5; 6], 4, 6, 2)
    case(h, [4; 5; 6], [0; 1; 2; 3; 4; 5; 6], 4, 7, 4)
    case(h, [4; 5; 6], [0; 1; 2; 3; 4; 5; 6], 4 where space = 4)
    case(h, Array[U8], [0; 1; 2; 3; 4; 5; 6], 4, 4, 0)
    case(h, Array[U8], [0; 1; 2; 3; 4; 5; 6], 4, 1, 0)
    case(h, Array[U8], Array[U8].init(8, 1024), 1024)

  fun case(
    h: TestHelper,
    expected: Array[U8],
    orig: Array[U8],
    from: USize,
    to: USize = -1,
    space: USize = 0)
  =>
    let copy: Array[U8] ref = orig.clone()
    let pre_trim_pagemap = @ponyint_pagemap_get[Pointer[None]](copy.cpointer())
    copy.trim_in_place(from, to)
    h.assert_eq[USize](space, copy.space())
    h.assert_array_eq[U8](expected, copy)
    let post_trim_pagemap = @ponyint_pagemap_get[Pointer[None]](copy.cpointer())
    if copy.space() == 0 then
      h.assert_eq[USize](0, post_trim_pagemap.usize())
    else
      h.assert_eq[USize](pre_trim_pagemap.usize(), post_trim_pagemap.usize())
    end

class iso _TestArrayTrimInPlaceWithAppend is UnitTest
  """
  Test trimming part of a array in place then append and trim again

  Verifies we don't get a segfault similar to String and...
  https://github.com/ponylang/ponyc/issues/1996
  """
  fun name(): String => "builtin/Array.trim_in_place_with_append"

  fun apply(h: TestHelper) =>
    let a: Array[U8] = [0; 1; 2; 3; 4; 5; 6]
    let big: Array[U8] val = recover val Array[U8].init(U8(1), 12_000) end
    a.trim_in_place(a.size())
    h.assert_array_eq[U8]([], a)
    a.append(big)
    a.trim_in_place(a.size())
    h.assert_array_eq[U8]([], a)
    a.append([0; 10])
    h.assert_array_eq[U8]([0; 10], a)

class iso _TestArrayInsert is UnitTest
  """
  Test inserting new element into array
  """
  fun name(): String => "builtin/Array.insert"

  fun apply(h: TestHelper) ? =>
    let a = ["one"; "three"]
    a.insert(0, "zero")?
    h.assert_array_eq[String](["zero"; "one"; "three"], a)

    let b = ["one"; "three"]
    b.insert(1, "two")?
    h.assert_array_eq[String](["one"; "two"; "three"], b)

    let c = ["one"; "three"]
    c.insert(2, "four")?
    h.assert_array_eq[String](["one"; "three"; "four"], c)

    h.assert_error({() ? => ["one"; "three"].insert(3, "invalid")? })

class iso _TestArrayValuesRewind is UnitTest
  """
  Test rewinding an ArrayValues object
  """
  fun name(): String => "builtin/ArrayValues.rewind"

  fun apply(h: TestHelper) ? =>
    let av = [as U32: 1; 2; 3; 4].values()

    h.assert_eq[U32](1, av.next()?)
    h.assert_eq[U32](2, av.next()?)
    h.assert_eq[U32](3, av.next()?)
    h.assert_eq[U32](4, av.next()?)
    h.assert_eq[Bool](false, av.has_next())

    av.rewind()

    h.assert_eq[Bool](true, av.has_next())
    h.assert_eq[U32](1, av.next()?)
    h.assert_eq[U32](2, av.next()?)
    h.assert_eq[U32](3, av.next()?)
    h.assert_eq[U32](4, av.next()?)
    h.assert_eq[Bool](false, av.has_next())

class _FindTestCls
  let i: ISize
  new create(i': ISize = 0) => i = i'
  fun eq(other: _FindTestCls box): Bool => i == other.i

class iso _TestArrayFind is UnitTest
  """
  Test finding elements in an array.
  """
  fun name(): String => "builtin/Array.find"

  fun apply(h: TestHelper) ? =>
    let a = [as ISize: 0; 1; 2; 3; 4; 1]
    h.assert_eq[USize](1, a.find(1)?)
    h.assert_eq[USize](5, a.find(1 where offset = 3)?)
    h.assert_eq[USize](5, a.find(1 where nth = 1)?)
    h.assert_error({() ? => a.find(6)? })
    h.assert_eq[USize](2, a.find(1 where predicate = {(l, r) => l > r })?)
    h.assert_eq[USize](0, a.find(0 where
      predicate = {(l, r) => (l % 3) == r })?)
    h.assert_eq[USize](3, a.find(0 where
      predicate = {(l, r) => (l % 3) == r }, nth = 1)?)
    h.assert_error(
      {() ? =>
        a.find(0 where predicate = {(l, r) => (l % 3) == r }, nth = 2)?
      })

    h.assert_eq[USize](5, a.rfind(1)?)
    h.assert_eq[USize](1, a.rfind(1 where offset = 3)?)
    h.assert_eq[USize](1, a.rfind(1 where nth = 1)?)
    h.assert_error({() ? => a.rfind(6)? })
    h.assert_eq[USize](4, a.rfind(1 where predicate = {(l, r) => l > r })?)
    h.assert_eq[USize](3, a.rfind(0 where
      predicate = {(l, r) => (l % 3) == r })?)
    h.assert_eq[USize](0, a.rfind(0 where
      predicate = {(l, r) => (l % 3) == r }, nth = 1)?)
    h.assert_error(
      {() ? =>
        a.rfind(0 where
          predicate = {(l, r) => (l % 3) == r },
          nth = 2)?
      })

    var b = Array[_FindTestCls]
    let c = _FindTestCls
    b.push(c)
    h.assert_error({() ? => b.find(_FindTestCls)? })
    h.assert_eq[USize](0, b.find(c)?)
    h.assert_eq[USize](0, b.find(_FindTestCls where
      predicate = {(l, r) => l == r })?)

class iso _TestArraySwapElements is UnitTest
  """
  Test swapping array elements
  """

  fun name(): String => "builtin/Array.swap_elements"

  fun apply(h: TestHelper)? =>
    let array = [as I32: 0; 1]
    let expected = [as I32: 1; 0]
    array.swap_elements(0, 1)?
    h.assert_array_eq[I32](expected, array)

    array.swap_elements(0, 0)?
    h.assert_array_eq[I32](expected, array)

    let singleton = [as I32: 42]
    singleton.swap_elements(0, 0)?
    h.assert_array_eq[I32]([as I32: 42], singleton)

    _test_error_cases(h)

  fun _test_error_cases(h: TestHelper) =>
    h.assert_error({()? =>
      Array[I32](0).swap_elements(0, 0)?
    })
    h.assert_error({()? =>
      Array[I32](0).swap_elements(0, 1)?
    })
    h.assert_error({()? =>
      Array[I32](0).swap_elements(1, 0)?
    })
    h.assert_error({()? =>
      [as I32: 1; 2; 3].swap_elements(3, 4)?
    })

class iso _TestArrayChop is UnitTest
  """
  Test chopping an array
  """
  fun name(): String => "builtin/Array.chop"

  fun apply(h: TestHelper) =>
    case(h, [0; 1; 2; 3], [4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 4)
    case(h, [0; 1; 2; 3; 4; 5], [6], recover [0; 1; 2; 3; 4; 5; 6] end, 6)
    case(h, [0], [1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 1)
    case(h, [0; 1; 2; 3; 4; 5; 6], Array[U8], recover [0; 1; 2; 3; 4; 5; 6] end, 7)
    case(h, Array[U8], [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 0)
    case(h, [0; 1; 2; 3; 4; 5; 6], Array[U8], recover [0; 1; 2; 3; 4; 5; 6] end, 10)

  fun case(
    h: TestHelper,
    expected_left: Array[U8],
    expected_right: Array[U8],
    orig: Array[U8] iso,
    split_point: USize)
  =>
    (let left: Array[U8] iso, let right: Array[U8] iso) = (consume orig).chop(split_point)
    h.assert_array_eq[U8](expected_left, consume left)
    h.assert_array_eq[U8](expected_right, consume right)

class iso _TestArrayUnchop is UnitTest
  """
  Test unchopping an array
  """
  fun name(): String => "builtin/Array.unchop"

  fun apply(h: TestHelper) =>
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 4, true)
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 6, true)
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 1, true)
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 7, true)
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 0, false)
    case(h, [0; 1; 2; 3; 4; 5; 6], recover [0; 1; 2; 3; 4; 5; 6] end, 10, true)
    case(h, [0; 1; 2; 3; 4; 5; 6; 7; 8; 9
             0; 1; 2; 3; 4; 5; 6; 7; 8; 9
             0; 1; 2; 3; 4; 5; 6; 7; 8; 9], recover
            [0; 1; 2; 3; 4; 5; 6; 7; 8; 9
             0; 1; 2; 3; 4; 5; 6; 7; 8; 9
             0; 1; 2; 3; 4; 5; 6; 7; 8; 9] end, None, true)

  fun case(
    h: TestHelper,
    orig: Array[U8],
    orig_clone1: Array[U8] iso,
    split_point: (USize | None),
    is_true: Bool)
  =>
    let arr_tag: Array[U8] tag = orig_clone1
    try
      let unchopped = chop_unchop(consume orig_clone1, split_point)?
      if is_true then
        h.assert_is[Array[U8] tag](arr_tag, unchopped)
      else
        h.assert_isnt[Array[U8] tag](arr_tag, unchopped)
      end
      h.assert_array_eq[U8](orig, consume unchopped)
    else
      h.assert_array_eq[U8](orig, Array[U8])
    end

  fun chop_unchop(x: Array[U8] iso,
    split_point: (USize | None)): Array[U8] iso^ ?
  =>
    let sp =
      match split_point
      | None => x.size()/2
        if x.size() < 4 then
          return consume x
        end
        x.size()/2
      | let z: USize => z
    end

    (let left: Array[U8] iso, let right: Array[U8] iso) = (consume x).chop(sp)
    let unchopped = (chop_unchop(consume left, None)?)
      .unchop(chop_unchop(consume right, None)?)

    match consume unchopped
    | let y: Array[U8] iso =>
      (let left2: Array[U8] iso, let right2: Array[U8] iso) =
        (consume y).chop(sp)
      let unchopped2 = (chop_unchop(consume left2, None)?)
        .unchop(chop_unchop(consume right2, None)?)

      match consume unchopped2
      | let z: Array[U8] iso => return consume z
      else
        error
      end
    else
      error
    end

class iso _TestMath128 is UnitTest
  """
  Test 128 bit integer math.
  """
  fun name(): String => "builtin/Math128"

  fun apply(h: TestHelper) =>
    h.assert_eq[F64](0, U128(0).f64())
    h.assert_eq[F64](1, U128(1).f64())
    h.assert_eq[F64](1e10, U128(10_000_000_000).f64())
    h.assert_eq[F64](1e20, U128(100_000_000_000_000_000_000).f64())

    h.assert_eq[F64](0, I128(0).f64())
    h.assert_eq[F64](1, I128(1).f64())
    h.assert_eq[F64](1e10, I128(10_000_000_000).f64())
    h.assert_eq[F64](1e20, I128(100_000_000_000_000_000_000).f64())

    h.assert_eq[F64](-1, I128(-1).f64())
    h.assert_eq[F64](-1e10, I128(-10_000_000_000).f64())
    h.assert_eq[F64](-1e20, I128(-100_000_000_000_000_000_000).f64())

    h.assert_eq[I128](0, 0 * 3)
    h.assert_eq[I128](8, 2 * 4)
    h.assert_eq[I128](1_000_000_000_000, 1_000_000 * 1_000_000)
    h.assert_eq[I128](100_000_000_000_000_000_000,
      10_000_000_000 * 10_000_000_000)

    h.assert_eq[I128](-8, -2 * 4)
    h.assert_eq[I128](-1_000_000_000_000, -1_000_000 * 1_000_000)
    h.assert_eq[I128](-100_000_000_000_000_000_000,
      -10_000_000_000 * 10_000_000_000)

    h.assert_eq[I128](8, -2 * -4)
    h.assert_eq[I128](1_000_000_000_000, -1_000_000 * -1_000_000)
    h.assert_eq[I128](100_000_000_000_000_000_000,
      -10_000_000_000 * -10_000_000_000)

    // Stop compiler moaning about dividing by constant 0.
    var uzero = U128(0)
    var izero = I128(0)

    // division
    h.assert_eq[U128](0, 100 / uzero)
    h.assert_eq[U128](2, 8 / 4)
    h.assert_eq[U128](1_000_000, 1_000_000_000_000 / 1_000_000)
    h.assert_eq[U128](10_000_000_000,
      100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, 100 / izero)
    h.assert_eq[I128](2, 8 / 4)
    h.assert_eq[I128](1_000_000, 1_000_000_000_000 / 1_000_000)
    h.assert_eq[I128](10_000_000_000,
      100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, -100 / izero)
    h.assert_eq[I128](-2, -8 / 4)
    h.assert_eq[I128](-1_000_000, -1_000_000_000_000 / 1_000_000)
    h.assert_eq[I128](-10_000_000_000,
      -100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, -100 / -izero)
    h.assert_eq[I128](2, -8 / -4)
    h.assert_eq[I128](1_000_000, -1_000_000_000_000 / -1_000_000)
    h.assert_eq[I128](10_000_000_000,
      -100_000_000_000_000_000_000 / -10_000_000_000)

    // remainder
    h.assert_eq[U128](0, 100 % uzero)
    h.assert_eq[U128](5, 13 % 8)
    h.assert_eq[U128](28, 40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](0, 100 % izero)
    h.assert_eq[I128](5, 13 % 8)
    h.assert_eq[I128](28, 40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](-5, -13 % 8)
    h.assert_eq[I128](-28, -40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](5, 13 % -8)
    h.assert_eq[I128](28, 40_000_000_028 % -10_000_000_000)

    h.assert_eq[I128](-5, -13 % -8)
    h.assert_eq[I128](-28, -40_000_000_028 % -10_000_000_000)

    // floored division
    h.assert_eq[U128](0, U128(100).fld(uzero))
    h.assert_eq[U128](2, U128(8).fld(4))
    h.assert_eq[U128](1_000_000, U128(1_000_000_000_000).fld(1_000_000))
    h.assert_eq[U128](10_000_000_000,
      U128(100_000_000_000_000_000_000).fld(10_000_000_000))

    h.assert_eq[I128](0, I128(100).fld(izero))
    h.assert_eq[I128](2, I128(8).fld(4))
    h.assert_eq[I128](1_000_000, I128(1_000_000_000_000).fld(1_000_000))
    h.assert_eq[I128](10_000_000_000,
      I128(100_000_000_000_000_000_000).fld(10_000_000_000))

    h.assert_eq[I128](0, I128(-100).fld(izero))
    h.assert_eq[I128](-2, I128(-8).fld(4))
    h.assert_eq[I128](-1_000_000, I128(-1_000_000_000_000).fld(1_000_000))
    h.assert_eq[I128](-10_000_000_000,
      I128(-100_000_000_000_000_000_000).fld(10_000_000_000))

    h.assert_eq[I128](0, I128(-100).fld(-izero))
    h.assert_eq[I128](2, I128(-8).fld(-4))
    h.assert_eq[I128](1_000_000, I128(-1_000_000_000_000).fld(-1_000_000))
    h.assert_eq[I128](10_000_000_000,
      I128(-100_000_000_000_000_000_000).fld(-10_000_000_000))

    // modulo
    h.assert_eq[U128](0, U128(100) %% uzero)
    h.assert_eq[U128](5, U128(13) %% 8)
    h.assert_eq[U128](28, U128(40_000_000_028) %% 10_000_000_000)

    h.assert_eq[I128](0, I128(100) %% izero)
    h.assert_eq[I128](5, I128(13) %% 8)
    h.assert_eq[I128](28, I128(40_000_000_028) %% 10_000_000_000)

    h.assert_eq[I128](3, I128(-13) %% 8)
    h.assert_eq[I128](9999999972, I128(-40_000_000_028) %% 10_000_000_000)

    h.assert_eq[I128](-3, I128(13) %% -8)
    h.assert_eq[I128](-9999999972, I128(40_000_000_028) %% -10_000_000_000)

    h.assert_eq[I128](-5, I128(-13) %% -8)
    h.assert_eq[I128](-28, I128(-40_000_000_028) %% -10_000_000_000)

class iso _TestRem is UnitTest
  """
  Test rem on various bit widths.
  """
  fun name(): String => "builtin/Rem"

  fun test_rem_signed[T: (Integer[T] val & Signed)](h: TestHelper) =>
    // special cases
    h.assert_eq[T](0, T(13) % T(0))
    h.assert_eq[T](0, T.min_value() % -1)

    h.assert_eq[T](5, T(13) % 8)
    h.assert_eq[T](-5, T(-13) % 8)
    h.assert_eq[T](5, T(13) % -8)
    h.assert_eq[T](-5, T(-13) % -8)

  fun test_rem_unsigned[T: (Integer[T] val & Unsigned)](h: TestHelper) =>
    h.assert_eq[T](0, T(13) % T(0))
    h.assert_eq[T](5, T(13) % 8)

  fun apply(h: TestHelper) =>
    test_rem_signed[I8](h)
    test_rem_signed[I16](h)
    test_rem_signed[I32](h)
    test_rem_signed[I64](h)
    test_rem_signed[ISize](h)
    test_rem_signed[ILong](h)
    test_rem_signed[I128](h)

    test_rem_unsigned[U8](h)
    test_rem_unsigned[U16](h)
    test_rem_unsigned[U32](h)
    test_rem_unsigned[U64](h)
    test_rem_unsigned[USize](h)
    test_rem_unsigned[ULong](h)
    test_rem_unsigned[U128](h)

class iso _TestFld is UnitTest
  fun name(): String => "builtin/Fld"

  fun test_fld_signed[T: (Integer[T] val & Signed)](h: TestHelper, type_name: String) =>
    h.assert_eq[T](0, T(11).fld(T(0)), "[" + type_name + "] 11 fld 0")
    h.assert_eq[T](0, T(-11).fld(T(0)), "[" + type_name + "] -11 fld 0")
    h.assert_eq[T](0, T(0).fld(T(11)), "[" + type_name + "] 0 fld 11")
    h.assert_eq[T](0, T(0).fld(T(-11)), "[" + type_name + "] 0 fld -11")
    h.assert_eq[T](0, T.min_value().fld(T(-1)), "[" + type_name + "] MIN fld -1")

    h.assert_eq[T](-2, T(-12).fld(T(8)), "[" + type_name + "] -12 fld 8")
    h.assert_eq[T](-2, T(12).fld(T(-8)), "[" + type_name + "] 12 fld -8")
    h.assert_eq[T](1, T(12).fld(T(8)), "[" + type_name + "] 12 fld 8")

    // fld_unsafe
    h.assert_eq[T](1, T(13).fld_unsafe(T(8)), "[" + type_name + "] 13 fld_unsafe 8")
    h.assert_eq[T](-2, T(-13).fld_unsafe(T(8)), "[" + type_name + "] -13 fld_unsafe 8")
    h.assert_eq[T](-2, T(13).fld_unsafe(T(-8)), "[" + type_name + "] 13 fld_unsafe -8")
    h.assert_eq[T](1, T(-13).fld_unsafe(T(-8)), "[" + type_name + "] -13 fld_unsafe 8")

  fun test_fld_unsigned[T: (Integer[T] val & Unsigned)](h: TestHelper, type_name: String) =>
    h.assert_eq[T](0, T(11).fld(T(0)), "[" + type_name + "] 11 fld 0")
    h.assert_eq[T](0, T(0).fld(T(11)), "[" + type_name + "] 0 fld 11")

    h.assert_eq[T](1, T(12).fld(T(8)), "[" + type_name + "] 12 fld 8")

    // fld_unsafe
    h.assert_eq[T](1, T(13).fld_unsafe(T(8)), "[" + type_name + "] 13 fld_unsafe 8")

  fun apply(h: TestHelper) =>
    test_fld_signed[I8](h, "I8")
    test_fld_signed[I16](h, "I16")
    test_fld_signed[I32](h, "I32")
    test_fld_signed[I64](h, "I64")
    test_fld_signed[ILong](h, "ILong")
    test_fld_signed[ISize](h, "ISize")
    test_fld_signed[I128](h, "I128")

    test_fld_unsigned[U8](h, "U8")
    test_fld_unsigned[U16](h, "U16")
    test_fld_unsigned[U32](h, "U32")
    test_fld_unsigned[U64](h, "U64")
    test_fld_unsigned[ULong](h, "ULong")
    test_fld_unsigned[USize](h, "USize")
    test_fld_unsigned[U128](h, "U128")

class iso _TestMod is UnitTest
  fun name(): String => "builtin/Mod"

  fun test_mod_signed[T: (Integer[T] val & Signed)](h: TestHelper, type_name: String) =>
    // mod
    // special cases
    h.assert_eq[T](0, T(13) %% T(0), "[" + type_name + "] 13 %% 0")
    h.assert_eq[T](0, T(-13) %% T(0), "[" + type_name + "] -13 %% 0")
    h.assert_eq[T](0, T(0) %% T(-13), "[" + type_name + "] 0 %% -13")
    h.assert_eq[T](0, T.min_value() %% -1, "[" + type_name + "] MIN %% -1")

    h.assert_eq[T](5, T(13) %% 8, "[" + type_name + "] 13 %% 8")
    h.assert_eq[T](3, T(-13) %% 8, "[" + type_name + "] -13 %% 8")
    h.assert_eq[T](-3, T(13) %% -8, "[" + type_name + "] 13 %% -8")
    h.assert_eq[T](-5, T(-13) %% -8, "[" + type_name + "] -13 %% -8")

    // mod_unsafe
    h.assert_eq[T](5, T(13) %%~ 8, "[" + type_name + "] 13 %%~ 8")
    h.assert_eq[T](3, T(-13) %%~ 8, "[" + type_name + "] -13 %%~ 8")
    h.assert_eq[T](-3, T(13) %%~ -8, "[" + type_name + "] 13 %%~ -8")
    h.assert_eq[T](-5, T(-13) %%~ -8, "[" + type_name + "] -13 %%~ -8")

  fun test_mod_unsigned[T: (Integer[T] val & Unsigned)](h: TestHelper, type_name: String) =>
    h.assert_eq[T](0, T(13) %% T(0), "[" + type_name + "] 13 %% 0")
    h.assert_eq[T](0, T(0) %% T(13), "[" + type_name + "] 0 %% 13")
    h.assert_eq[T](5, T(13) %% T(8), "[" + type_name + "] 13 %% 8")

    // mod_unsafe
    h.assert_eq[T](5, T(13) %%~ 8, "[" + type_name + "] 13 %%~ 8")

  fun test_mod_float[T: (FloatingPoint[T] val & Float)](h: TestHelper, type_name: String) ? =>
    let inf = T.from[F64]("Inf".f64()?)
    let minf = T.from[F64]("-Inf".f64()?)
    let nan = T.from[F64]("NaN".f64()?)

    // special cases - 0
    h.assert_true((T(1.5) %% T(0.0)).nan(), "[" + type_name + "] 1.5 %% 0.0")
    h.assert_true((T(-1.5) %% T(0.0)).nan(), "[" + type_name + "] -1.5 %% 0.0")
    h.assert_eq[T](T(0.0), T(0.0) %% T(1.5), "[" + type_name + "] 0.0 %% 1.5")

    // with nan
    h.assert_true((nan %% T(1.5)).nan(), "[" + type_name + "] NaN %% 1.5")
    h.assert_true((T(1.5) %% nan).nan(), "[" + type_name + "] 1.5 %% NaN")

    // with inf
    h.assert_true((inf %% T(1.5)).nan(), "[" + type_name + "] inf %% 1.5")
    h.assert_true((inf %% T(-1.5)).nan(), "[" + type_name + "] inf %% -1.5")
    h.assert_eq[T](T(1.5), T(1.5) %% inf, "[" + type_name + "] 1.5 %% inf")
    h.assert_true((minf %% T(1.5)).nan(), "[" + type_name + "] -inf %% 1.5")
    h.assert_true((minf %% T(-1.5)).nan(), "[" + type_name + "] -inf %% -1.5")
    h.assert_true((T(1.5) %% minf).infinite(), "[" + type_name + "] 1.5 %% -inf")

    h.assert_eq[T](T(7.0), T(15.0) %% T(8.0), "[" + type_name + "] 15.0 %% 8.0")
    h.assert_eq[T](T(1.0), T(-15.0) %% T(8.0), "[" + type_name + "] -15.0 %% 8.0")
    h.assert_eq[T](T(-1.0), T(15.0) %% T(-8.0), "[" + type_name + "] 15.0 %% -8.0")
    h.assert_eq[T](T(-7.0), T(-15.0) %% T(-8.0), "[" + type_name + "] -15.0 %% -8.0")

    // mod_unsafe
    h.assert_eq[T](T(7.0), T(15.0) %%~ T(8.0), "[" + type_name + "] 15.0 %%~ 8.0")
    h.assert_eq[T](T(1.0), T(-15.0) %%~ T(8.0), "[" + type_name + "] -15.0 %%~ 8.0")
    h.assert_eq[T](T(-1.0), T(15.0) %%~ T(-8.0), "[" + type_name + "] 15.0 %%~ -8.0")
    h.assert_eq[T](T(-7.0), T(-15.0) %%~ T(-8.0), "[" + type_name + "] -15.0 %%~ -8.0")

  fun apply(h: TestHelper) ? =>
    test_mod_signed[I8](h, "I8")
    test_mod_signed[I16](h, "I16")
    test_mod_signed[I32](h, "I32")
    test_mod_signed[I64](h, "I64")
    test_mod_signed[ISize](h, "ISize")
    test_mod_signed[ILong](h, "ILong")
    test_mod_signed[I128](h, "I128")

    test_mod_unsigned[U8](h, "U8")
    test_mod_unsigned[U16](h, "U16")
    test_mod_unsigned[U32](h, "U32")
    test_mod_unsigned[USize](h, "USize")
    test_mod_unsigned[ULong](h, "ULong")
    test_mod_unsigned[U128](h, "U128")

    test_mod_float[F32](h, "F32")?
    test_mod_float[F64](h, "F64")?

trait iso SafeArithmeticTest is UnitTest
  fun test[A: (Equatable[A] #read & Stringable #read)](
    h: TestHelper,
    expected: (A, Bool),
    actual: (A, Bool),
    loc: SourceLoc = __loc)
  =>
    h.assert_eq[A](expected._1, actual._1 where loc=loc)
    h.assert_eq[Bool](expected._2, actual._2 where loc=loc)

  fun test_overflow[A: (Equatable[A] #read & Stringable #read)](
    h: TestHelper,
    actual: (A, Bool),
    loc: SourceLoc = __loc)
  =>
    h.assert_eq[Bool](true, actual._2)

class iso _TestAddc is SafeArithmeticTest
  """
  Test addc on various bit widths.
  """
  fun name(): String => "builtin/Addc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0xff, false), U8(0xfe).addc(1))
    test[U8](h, (0, true), U8(0xff).addc(1))
    test[U8](h, (0xfe, true), U8(0xff).addc(0xff))

    test[U16](h, (0xffff, false), U16(0xfffe).addc(1))
    test[U16](h, (0, true), U16(0xffff).addc(1))
    test[U16](h, (0xfffe, true), U16(0xffff).addc(0xffff))

    test[U32](h, (0xffff_ffff, false), U32(0xffff_fffe).addc(1))
    test[U32](h, (0, true), U32(0xffff_ffff).addc(1))
    test[U32](h, (0xffff_fffe, true), U32(0xffff_ffff).addc(0xffff_ffff))

    test[U64](h, (0xffff_ffff_ffff_ffff, false),
      U64(0xffff_ffff_ffff_fffe).addc(1))
    test[U64](h, (0, true),
      U64(0xffff_ffff_ffff_ffff).addc(1))
    test[U64](h, (0xffff_ffff_ffff_fffe, true),
      U64(0xffff_ffff_ffff_ffff).addc(0xffff_ffff_ffff_ffff))

    test[U128](h, (0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, false),
      U128(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_fffe).addc(1))
    test[U128](h, (0, true),
      U128(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).addc(1))
    test[U128](h, (0xffff_ffff_ffff_ffff_ffff_ffff_ffff_fffe, true),
      U128(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).addc(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff))

    test[I8](h, (0x7f, false), I8( 0x7e).addc( 1))
    test[I8](h, (-0x80, false), I8(-0x7f).addc(-1))
    test[I8](h, (-0x80, true), I8( 0x7f).addc( 1))
    test[I8](h, (0x7f, true), I8(-0x80).addc(-1))

    test[I16](h, (0x7fff, false), I16( 0x7ffe).addc( 1))
    test[I16](h, (-0x8000, false), I16(-0x7fff).addc(-1))
    test[I16](h, (-0x8000, true), I16( 0x7fff).addc( 1))
    test[I16](h, (0x7fff, true), I16(-0x8000).addc(-1))

    test[I32](h, (0x7fff_ffff, false), I32( 0x7fff_fffe).addc( 1))
    test[I32](h, (-0x8000_0000, false), I32(-0x7fff_ffff).addc(-1))
    test[I32](h, (-0x8000_0000, true), I32( 0x7fff_ffff).addc( 1))
    test[I32](h, (0x7fff_ffff, true), I32(-0x8000_0000).addc(-1))

    test[I64](h, (0x7fff_ffff_ffff_ffff, false),
      I64( 0x7fff_ffff_ffff_fffe).addc( 1))
    test[I64](h, (-0x8000_0000_0000_0000, false),
      I64(-0x7fff_ffff_ffff_ffff).addc(-1))
    test[I64](h, (-0x8000_0000_0000_0000, true),
      I64( 0x7fff_ffff_ffff_ffff).addc( 1))
    test[I64](h, (0x7fff_ffff_ffff_ffff, true),
      I64(-0x8000_0000_0000_0000).addc(-1))

    test[I128](h, (0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, false),
      I128( 0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_fffe).addc( 1))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, false),
      I128(-0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).addc(-1))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, true),
      I128( 0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).addc( 1))
    test[I128](h, (0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, true),
      I128(-0x8000_0000_0000_0000_0000_0000_0000_0000).addc(-1))

class iso _TestSubc is SafeArithmeticTest
  """
  Test addc on various bit widths.
  """
  fun name(): String => "builtin/Subc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0, false), U8(1).subc(1))
    test[U8](h, (0xff, true), U8(0).subc(1))
    test[U8](h, (1, true), U8(0).subc(0xff))

    test[U16](h, (0, false), U16(1).subc(1))
    test[U16](h, (0xffff, true), U16(0).subc(1))
    test[U16](h, (1, true), U16(0).subc(0xffff))

    test[U32](h, (0, false), U32(1).subc(1))
    test[U32](h, (0xffff_ffff, true), U32(0).subc(1))
    test[U32](h, (1, true), U32(0).subc(0xffff_ffff))

    test[U64](h, (0, false), U64(1).subc(1))
    test[U64](h, (0xffff_ffff_ffff_ffff, true), U64(0).subc(1))
    test[U64](h, (1, true), U64(0).subc(0xffff_ffff_ffff_ffff))

    test[U128](h, (0, false), U128(1).subc(1))
    test[U128](h, (0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, true),
      U128(0).subc(1))
    test[U128](h, (1, true),
      U128(0).subc(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff))

    test[I8](h, (0x7f, false), I8( 0x7e).subc(-1))
    test[I8](h, (-0x80, false), I8(-0x7f).subc( 1))
    test[I8](h, (-0x80, true), I8( 0x7f).subc(-1))
    test[I8](h, (0x7f, true), I8(-0x80).subc( 1))

    test[I16](h, (0x7fff, false), I16( 0x7ffe).subc(-1))
    test[I16](h, (-0x8000, false), I16(-0x7fff).subc( 1))
    test[I16](h, (-0x8000, true), I16( 0x7fff).subc(-1))
    test[I16](h, (0x7fff, true), I16(-0x8000).subc( 1))

    test[I32](h, (0x7fff_ffff, false), I32( 0x7fff_fffe).subc(-1))
    test[I32](h, (-0x8000_0000, false), I32(-0x7fff_ffff).subc( 1))
    test[I32](h, (-0x8000_0000, true), I32( 0x7fff_ffff).subc(-1))
    test[I32](h, (0x7fff_ffff, true), I32(-0x8000_0000).subc( 1))

    test[I64](h, (0x7fff_ffff_ffff_ffff, false),
      I64( 0x7fff_ffff_ffff_fffe).subc(-1))
    test[I64](h, (-0x8000_0000_0000_0000, false),
      I64(-0x7fff_ffff_ffff_ffff).subc( 1))
    test[I64](h, (-0x8000_0000_0000_0000, true),
      I64( 0x7fff_ffff_ffff_ffff).subc(-1))
    test[I64](h, (0x7fff_ffff_ffff_ffff, true),
      I64(-0x8000_0000_0000_0000).subc( 1))

    test[I128](h, (0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, false),
      I128( 0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_fffe).subc(-1))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, false),
      I128(-0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).subc( 1))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, true),
      I128( 0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).subc(-1))
    test[I128](h, (0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, true),
      I128(-0x8000_0000_0000_0000_0000_0000_0000_0000).subc( 1))

class iso _TestMulc is SafeArithmeticTest
  fun name(): String => "builtin/Mulc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0x80, false), U8(0x40).mulc(2))
    test[U8](h, (0, false), U8(0x40).mulc(0))
    test[U8](h, (0, true), U8(0x80).mulc(2))
    test[U8](h, (1, true), U8(0xff).mulc(0xff))

    test[U16](h, (0x8000, false), U16(0x4000).mulc(2))
    test[U16](h, (0, false), U16(0x4000).mulc(0))
    test[U16](h, (0, true), U16(0x8000).mulc(2))
    test[U16](h, (1, true), U16(0xffff).mulc(0xffff))

    test[U32](h, (0x8000_0000, false), U32(0x4000_0000).mulc(2))
    test[U32](h, (0, false), U32(0x4000_0000).mulc(0))
    test[U32](h, (0, true), U32(0x8000_0000).mulc(2))
    test[U32](h, (1, true), U32(0xffff_ffff).mulc(0xffff_ffff))

    test[U64](h, (0x8000_0000_0000_0000, false),
      U64(0x4000_0000_0000_0000).mulc(2))
    test[U64](h, (0, false), U64(0x4000_0000_0000_0000).mulc(0))
    test[U64](h, (0, true), U64(0x8000_0000_0000_0000).mulc(2))
    test[U64](h, (1, true),
      U64(0xffff_ffff_ffff_ffff).mulc(0xffff_ffff_ffff_ffff))

    test[U128](h, (0x8000_0000_0000_0000_0000_0000_0000_0000, false),
      U128(0x4000_0000_0000_0000_0000_0000_0000_0000).mulc(2))
    test[U128](h, (0, false),
      U128(0x4000_0000_0000_0000_0000_0000_0000_0000).mulc(0))
    test[U128](h, (0, true),
      U128(0x8000_0000_0000_0000_0000_0000_0000_0000).mulc(2))
    test[U128](h, (1, true),
      U128(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff)
        .mulc(0xffff_ffff_ffff_ffff_ffff_ffff_ffff_ffff))

    test[I8](h, (0x40,  false), I8(0x20).mulc( 2))
    test[I8](h, (-0x40, false), I8(0x20).mulc(-2))
    test[I8](h, (-0x7f, false), I8(0x7f).mulc(-1))
    test[I8](h, (-0x80, true),  I8(0x40).mulc( 2))
    test[I8](h, (-0x80, false), I8(0x40).mulc(-2))
    test[I8](h, (0x7e,  true),  I8(0x41).mulc(-2))

    test[I16](h, (0x4000,  false), I16(0x2000).mulc( 2))
    test[I16](h, (-0x4000, false), I16(0x2000).mulc(-2))
    test[I16](h, (-0x7fff, false), I16(0x7fff).mulc(-1))
    test[I16](h, (-0x8000, true),  I16(0x4000).mulc( 2))
    test[I16](h, (-0x8000, false), I16(0x4000).mulc(-2))
    test[I16](h, (0x7ffe,  true),  I16(0x4001).mulc(-2))

    test[I32](h, (0x4000_0000,  false), I32(0x2000_0000).mulc( 2))
    test[I32](h, (-0x4000_0000, false), I32(0x2000_0000).mulc(-2))
    test[I32](h, (-0x7fff_ffff, false), I32(0x7fff_ffff).mulc(-1))
    test[I32](h, (-0x8000_0000, true),  I32(0x4000_0000).mulc( 2))
    test[I32](h, (-0x8000_0000, false), I32(0x4000_0000).mulc(-2))
    test[I32](h, (0x7fff_fffe,  true),  I32(0x4000_0001).mulc(-2))

    test[I64](h, (0x4000_0000_0000_0000,  false),
      I64(0x2000_0000_0000_0000).mulc( 2))
    test[I64](h, (-0x4000_0000_0000_0000, false),
      I64(0x2000_0000_0000_0000).mulc(-2))
    test[I64](h, (-0x7fff_ffff_ffff_ffff, false),
      I64(0x7fff_ffff_ffff_ffff).mulc(-1))
    test[I64](h, (-0x8000_0000_0000_0000, true),
      I64(0x4000_0000_0000_0000).mulc( 2))
    test[I64](h, (-0x8000_0000_0000_0000, false),
      I64(0x4000_0000_0000_0000).mulc(-2))
    test[I64](h, (0x7fff_ffff_ffff_fffe,  true),
      I64(0x4000_0000_0000_0001).mulc(-2))

    test[I128](h, ( 0x4000_0000_0000_0000_0000_0000_0000_0000,  false),
      I128( 0x2000_0000_0000_0000_0000_0000_0000_0000).mulc( 2))
    test[I128](h, (-0x4000_0000_0000_0000_0000_0000_0000_0000, false),
      I128(0x2000_0000_0000_0000_0000_0000_0000_0000).mulc(-2))
    test[I128](h, (-0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff, false),
      I128(0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_ffff).mulc(-1))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, true),
      I128(0x4000_0000_0000_0000_0000_0000_0000_0000).mulc( 2))
    test[I128](h, (-0x8000_0000_0000_0000_0000_0000_0000_0000, false),
      I128(0x4000_0000_0000_0000_0000_0000_0000_0000).mulc(-2))
    test[I128](h, (0x7fff_ffff_ffff_ffff_ffff_ffff_ffff_fffe,  true),
      I128(0x4000_0000_0000_0000_0000_0000_0000_0001).mulc(-2))

class iso _TestDivc is SafeArithmeticTest
  fun name(): String => "builtin/Divc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0x20, false), U8(0x40).divc(2))
    test_overflow[U8](h, U8(0x40).divc(0))

    test[U16](h, (0x20, false), U16(0x40).divc(2))
    test_overflow[U16](h, U16(0x40).divc(0))

    test[U32](h, (0x20, false), U32(0x40).divc(2))
    test_overflow[U32](h, U32(0x40).divc(0))

    test[U64](h, (0x20, false), U64(0x40).divc(2))
    test_overflow[U64](h, U64(0x40).divc(0))

    test[ULong](h, (0x20, false), ULong(0x40).divc(2))
    test_overflow[ULong](h, ULong(0x40).divc(0))

    test[USize](h, (0x20, false), USize(0x40).divc(2))
    test_overflow[USize](h, USize(0x40).divc(0))

    test[U128](h, (0x20, false), U128(0x40).divc(2))
    test_overflow[U128](h, U128(0x40).divc(0))

    test[I8](h, (0x20, false), I8(0x40).divc(2))
    test_overflow[I8](h, I8(0x40).divc(0))
    test_overflow[I8](h, I8.min_value().divc(I8(-1)))

    test[I16](h, (0x20, false), I16(0x40).divc(2))
    test_overflow[I16](h, I16(0x40).divc(0))
    test_overflow[I16](h, I16.min_value().divc(I16(-1)))

    test[I32](h, (0x20, false), I32(0x40).divc(2))
    test_overflow[I32](h, I32(0x40).divc(0))
    test_overflow[I32](h, I32.min_value().divc(I32(-1)))

    test[I32](h, (0x20, false), I32(0x40).divc(2))
    test_overflow[I32](h, I32(0x40).divc(0))
    test_overflow[I32](h, I32.min_value().divc(I32(-1)))

    test[I64](h, (0x20, false), I64(0x40).divc(2))
    test_overflow[I64](h, I64(0x40).divc(0))
    test_overflow[I64](h, I64.min_value().divc(I64(-1)))

    test[ILong](h, (0x20, false), ILong(0x40).divc(2))
    test_overflow[ILong](h, ILong(0x40).divc(0))
    test_overflow[ILong](h, ILong.min_value().divc(ILong(-1)))

    test[ISize](h, (0x20, false), ISize(0x40).divc(2))
    test_overflow[ISize](h, ISize(0x40).divc(0))
    test_overflow[ISize](h, ISize.min_value().divc(ISize(-1)))

    test[I128](h, (0x20, false), I128(0x40).divc(2))
    test_overflow[I128](h, I128(0x40).divc(0))
    test_overflow[I128](h, I128.min_value().divc(I128(-1)))

class iso _TestFldc is SafeArithmeticTest
  fun name(): String => "builtin/Fldc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0x20, false), U8(0x40).fldc(2))
    test_overflow[U8](h, U8(0x40).fldc(0))

    test[U16](h, (0x20, false), U16(0x40).fldc(2))
    test_overflow[U16](h, U16(0x40).fldc(0))

    test[U32](h, (0x20, false), U32(0x40).fldc(2))
    test_overflow[U32](h, U32(0x40).fldc(0))

    test[U64](h, (0x20, false), U64(0x40).fldc(2))
    test_overflow[U64](h, U64(0x40).fldc(0))

    test[ULong](h, (0x20, false), ULong(0x40).fldc(2))
    test_overflow[ULong](h, ULong(0x40).fldc(0))

    test[USize](h, (0x20, false), USize(0x40).fldc(2))
    test_overflow[USize](h, USize(0x40).fldc(0))

    test[U128](h, (0x20, false), U128(0x40).fldc(2))
    test_overflow[U128](h, U128(0x40).fldc(0))

    test[I8](h, (0x20, false), I8(0x40).fldc(2))
    test_overflow[I8](h, I8(0x40).fldc(0))
    test_overflow[I8](h, I8.min_value().fldc(I8(-1)))

    test[I16](h, (0x20, false), I16(0x40).fldc(2))
    test_overflow[I16](h, I16(0x40).fldc(0))
    test_overflow[I16](h, I16.min_value().fldc(I16(-1)))

    test[I32](h, (0x20, false), I32(0x40).fldc(2))
    test_overflow[I32](h, I32(0x40).fldc(0))
    test_overflow[I32](h, I32.min_value().fldc(I32(-1)))

    test[I32](h, (0x20, false), I32(0x40).fldc(2))
    test_overflow[I32](h, I32(0x40).fldc(0))
    test_overflow[I32](h, I32.min_value().fldc(I32(-1)))

    test[I64](h, (0x20, false), I64(0x40).fldc(2))
    test_overflow[I64](h, I64(0x40).fldc(0))
    test_overflow[I64](h, I64.min_value().fldc(I64(-1)))

    test[ILong](h, (0x20, false), ILong(0x40).fldc(2))
    test_overflow[ILong](h, ILong(0x40).fldc(0))
    test_overflow[ILong](h, ILong.min_value().fldc(ILong(-1)))

    test[ISize](h, (0x20, false), ISize(0x40).fldc(2))
    test_overflow[ISize](h, ISize(0x40).fldc(0))
    test_overflow[ISize](h, ISize.min_value().fldc(ISize(-1)))

    test[I128](h, (0x20, false), I128(0x40).fldc(2))
    test_overflow[I128](h, I128(0x40).fldc(0))
    test_overflow[I128](h, I128.min_value().fldc(I128(-1)))

class iso _TestRemc is SafeArithmeticTest
  fun name(): String => "builtin/Remc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0x01, false), U8(0x41).remc(2))
    test_overflow[U8](h, U8(0x40).remc(0))

    test[U16](h, (0x01, false), U16(0x41).remc(2))
    test_overflow[U16](h, U16(0x40).remc(0))

    test[U32](h, (0x01, false), U32(0x41).remc(2))
    test_overflow[U32](h, U32(0x40).remc(0))

    test[U64](h, (0x01, false), U64(0x41).remc(2))
    test_overflow[U64](h, U64(0x40).remc(0))

    test[U128](h, (0x01, false), U128(0x41).remc(2))
    test_overflow[U128](h, U128(0x40).remc(0))

    test[ULong](h, (0x01, false), ULong(0x41).remc(2))
    test_overflow[ULong](h, ULong(0x40).remc(0))

    test[USize](h, (0x01, false), USize(0x41).remc(2))
    test_overflow[USize](h, USize(0x40).remc(0))

    test[I8](h, (0x01, false), I8(0x41).remc(2))
    test[I8](h, (-0x01, false), I8(-0x41).remc(2))
    test[I8](h, (-0x02, false), I8(-0x41).remc(-3))
    test_overflow[I8](h, I8(0x40).remc(0))
    test_overflow[I8](h, I8(-0x40).remc(0))
    test_overflow[I8](h, I8.min_value().remc(-1))

    test[I16](h, (0x01, false), I16(0x41).remc(2))
    test[I16](h, (-0x01, false), I16(-0x41).remc(2))
    test[I16](h, (-0x02, false), I16(-0x41).remc(-3))
    test_overflow[I16](h, I16(0x40).remc(0))
    test_overflow[I16](h, I16(-0x40).remc(0))
    test_overflow[I16](h, I16.min_value().remc(-1))

    test[I32](h, (0x01, false), I32(0x41).remc(2))
    test[I32](h, (-0x01, false), I32(-0x41).remc(2))
    test[I32](h, (-0x02, false), I32(-0x41).remc(-3))
    test_overflow[I32](h, I32(0x40).remc(0))
    test_overflow[I32](h, I32(-0x40).remc(0))
    test_overflow[I32](h, I32.min_value().remc(-1))

    test[I64](h, (0x01, false), I64(0x41).remc(2))
    test[I64](h, (-0x01, false), I64(-0x41).remc(2))
    test[I64](h, (-0x02, false), I64(-0x41).remc(-3))
    test_overflow[I64](h, I64(0x40).remc(0))
    test_overflow[I64](h, I64(-0x40).remc(0))
    test_overflow[I64](h, I64.min_value().remc(-1))

    test[I128](h, (0x01, false), I128(0x41).remc(2))
    test[I128](h, (-0x01, false), I128(-0x41).remc(2))
    test[I128](h, (-0x02, false), I128(-0x41).remc(-3))
    test_overflow[I128](h, I128(0x40).remc(0))
    test_overflow[I128](h, I128(-0x40).remc(0))
    test_overflow[I128](h, I128.min_value().remc(-1))

    test[ILong](h, (0x01, false), ILong(0x41).remc(2))
    test[ILong](h, (-0x01, false), ILong(-0x41).remc(2))
    test[ILong](h, (-0x02, false), ILong(-0x41).remc(-3))
    test_overflow[ILong](h, ILong(0x40).remc(0))
    test_overflow[ILong](h, ILong(-0x40).remc(0))
    test_overflow[ILong](h, ILong.min_value().remc(-1))

    test[ISize](h, (0x01, false), ISize(0x41).remc(2))
    test[ISize](h, (-0x01, false), ISize(-0x41).remc(2))
    test[ISize](h, (-0x02, false), ISize(-0x41).remc(-3))
    test_overflow[ISize](h, ISize(0x40).remc(0))
    test_overflow[ISize](h, ISize(-0x40).remc(0))
    test_overflow[ISize](h, ISize.min_value().remc(-1))

class iso _TestModc is SafeArithmeticTest
  fun name(): String => "builtin/Modc"

  fun apply(h: TestHelper) =>
    test[U8](h, (0x01, false), U8(0x41).modc(2))
    test_overflow[U8](h, U8(0x40).modc(0))

    test[U16](h, (0x01, false), U16(0x41).modc(2))
    test_overflow[U16](h, U16(0x40).modc(0))

    test[U32](h, (0x01, false), U32(0x41).modc(2))
    test_overflow[U32](h, U32(0x40).modc(0))

    test[U64](h, (0x01, false), U64(0x41).modc(2))
    test_overflow[U64](h, U64(0x40).modc(0))

    test[U128](h, (0x01, false), U128(0x41).modc(2))
    test_overflow[U128](h, U128(0x40).modc(0))

    test[ULong](h, (0x01, false), ULong(0x41).modc(2))
    test_overflow[ULong](h, ULong(0x40).modc(0))

    test[USize](h, (0x01, false), USize(0x41).modc(2))
    test_overflow[USize](h, USize(0x40).modc(0))

    test[I8](h, (0x01, false), I8(0x41).modc(2))
    test[I8](h, (0x01, false), I8(-0x41).modc(2))
    test[I8](h, (-0x02, false), I8(-0x41).modc(-3))
    test_overflow[I8](h, I8(0x40).modc(0))
    test_overflow[I8](h, I8(-0x40).modc(0))
    test_overflow[I8](h, I8.min_value().modc(-1))

    test[I16](h, (0x01, false), I16(0x41).modc(2))
    test[I16](h, (0x01, false), I16(-0x41).modc(2))
    test[I16](h, (-0x02, false), I16(-0x41).modc(-3))
    test_overflow[I16](h, I16(0x40).modc(0))
    test_overflow[I16](h, I16(-0x40).modc(0))
    test_overflow[I16](h, I16.min_value().modc(-1))

    test[I32](h, (0x01, false), I32(0x41).modc(2))
    test[I32](h, (0x01, false), I32(-0x41).modc(2))
    test[I32](h, (-0x02, false), I32(-0x41).modc(-3))
    test_overflow[I32](h, I32(0x40).modc(0))
    test_overflow[I32](h, I32(-0x40).modc(0))
    test_overflow[I32](h, I32.min_value().modc(-1))

    test[I64](h, (0x01, false), I64(0x41).modc(2))
    test[I64](h, (0x01, false), I64(-0x41).modc(2))
    test[I64](h, (-0x02, false), I64(-0x41).modc(-3))
    test_overflow[I64](h, I64(0x40).modc(0))
    test_overflow[I64](h, I64(-0x40).modc(0))
    test_overflow[I64](h, I64.min_value().modc(-1))

    test[I128](h, (0x01, false), I128(0x41).modc(2))
    test[I128](h, (0x01, false), I128(-0x41).modc(2))
    test[I128](h, (-0x02, false), I128(-0x41).modc(-3))
    test_overflow[I128](h, I128(0x40).modc(0))
    test_overflow[I128](h, I128(-0x40).modc(0))
    test_overflow[I128](h, I128.min_value().modc(-1))

    test[ILong](h, (0x01, false), ILong(0x41).modc(2))
    test[ILong](h, (0x01, false), ILong(-0x41).modc(2))
    test[ILong](h, (-0x02, false), ILong(-0x41).modc(-3))
    test_overflow[ILong](h, ILong(0x40).modc(0))
    test_overflow[ILong](h, ILong(-0x40).modc(0))
    test_overflow[ILong](h, ILong.min_value().modc(-1))

    test[ISize](h, (0x01, false), ISize(0x41).modc(2))
    test[ISize](h, (0x01, false), ISize(-0x41).modc(2))
    test[ISize](h, (-0x02, false), ISize(-0x41).modc(-3))
    test_overflow[ISize](h, ISize(0x40).modc(0))
    test_overflow[ISize](h, ISize(-0x40).modc(0))
    test_overflow[ISize](h, ISize.min_value().modc(-1))

primitive _CommonPartialArithmeticTests[T: (Integer[T] val & Int)]
  fun apply(h: TestHelper)? =>
    //addition
    h.assert_error({()? => T.max_value() +? T(1) })
    h.assert_eq[T](T.max_value(), T.max_value() +? T(0))

    // subtraction
    h.assert_error({()? => T.min_value() -? T(1) })
    h.assert_eq[T](T(3), T(10) -? T(7))

    // multiplication
    h.assert_error({()? => T.max_value() *? T(2) })
    h.assert_eq[T](T(30), T(10) *? T(3))

    // division
    h.assert_error({()? => T(1) /? T(0) })
    h.assert_eq[T](T(5), T(10) /? T(2))

    // floored division
    h.assert_error({()? => T(1).fld_partial(T(0))? })
    h.assert_eq[T](T(5), T(10).fld_partial(T(2))?)

    // remainder
    h.assert_error({()? => T(2) %? T(0) })
    h.assert_eq[T](T(1), T(11) %? T(2))

    // modulo
    h.assert_error({()? => T(2) %%? T(0) })
    h.assert_eq[T](T(1), T(11) %%? T(2))

    // divrem
    h.assert_error({()? => T(3).divrem_partial(T(0))? })
    (let divr, let remr) = T(11).divrem_partial(T(2))?
    h.assert_eq[T](T(5), divr)
    h.assert_eq[T](T(1), remr)

primitive _UnsignedPartialArithmeticTests[T: (Integer[T] val & Unsigned)]
  fun apply(h: TestHelper) =>
    // division
    h.assert_no_error({()? => T.min_value() /? T(-1) })

    // floored division
    h.assert_no_error({()? => T.min_value().fld_partial(T(-1))? })

    // remainder
    h.assert_no_error({()? => T.min_value() %? T(-1) })

    // modulo
    h.assert_no_error({()? => T.min_value() %%? T(-1) })
    h.assert_no_error({()? => h.assert_eq[T](5, T(13)  %%?  8) })

    // divrem
    h.assert_no_error({()? => T.min_value().divrem_partial(T(-1))? })

primitive _SignedPartialArithmeticTests[T: (Integer[T] val & Signed)]
  fun apply(h: TestHelper) =>
    // addition
    h.assert_error({()? => T.min_value() +? T(-1) })

    // subtraction
    h.assert_error({()? => T.max_value() -? T(-1) })

    // multiplication
    h.assert_error({()? => T.min_value() *? T(-2) })

    // division
    h.assert_error({()? => T.min_value() /? T(-1) })

    // floored division
    h.assert_error({()? => T.min_value().fld_partial(T(-1))? })

    // remainder
    h.assert_error({()? => T.min_value() %? T(-1) })

    // modulo
    h.assert_error({()? => T(-13) %%? T(0)})
    h.assert_error({()? => T.min_value() %%? -1 })

    h.assert_no_error(
      {()? =>
        h.assert_eq[T](3, T(-13) %%?  8)
      })
    h.assert_no_error(
      {()? =>
        h.assert_eq[T](-3, T(13)  %%? -8)
      })
    h.assert_no_error(
      {()? =>
        h.assert_eq[T](-5, T(-13) %%? -8)
      })

    // divrem
    h.assert_error({()? => T.min_value().divrem_partial(T(-1))? })

class iso _TestSignedPartialArithmetic is UnitTest
  fun name(): String => "builtin/PartialArithmetic/signed"

  fun apply(h: TestHelper)? =>
    _CommonPartialArithmeticTests[I8](h)?
    _SignedPartialArithmeticTests[I8](h)
    _CommonPartialArithmeticTests[I16](h)?
    _SignedPartialArithmeticTests[I16](h)
    _CommonPartialArithmeticTests[I32](h)?
    _SignedPartialArithmeticTests[I32](h)
    _CommonPartialArithmeticTests[I64](h)?
    _SignedPartialArithmeticTests[I64](h)
    _CommonPartialArithmeticTests[I128](h)?
    _SignedPartialArithmeticTests[I128](h)
    _CommonPartialArithmeticTests[ILong](h)?
    _SignedPartialArithmeticTests[ILong](h)
    _CommonPartialArithmeticTests[ISize](h)?
    _SignedPartialArithmeticTests[ISize](h)

class iso _TestUnsignedPartialArithmetic is UnitTest
  fun name(): String => "builtin/PartialArithmetic/unsigned"
  fun apply(h: TestHelper)? =>
    _CommonPartialArithmeticTests[U8](h)?
    _UnsignedPartialArithmeticTests[U8](h)
    _CommonPartialArithmeticTests[U16](h)?
    _UnsignedPartialArithmeticTests[U16](h)
    _CommonPartialArithmeticTests[U32](h)?
    _UnsignedPartialArithmeticTests[U32](h)
    _CommonPartialArithmeticTests[U64](h)?
    _UnsignedPartialArithmeticTests[U64](h)
    _CommonPartialArithmeticTests[U128](h)?
    _UnsignedPartialArithmeticTests[U128](h)
    _CommonPartialArithmeticTests[ULong](h)?
    _UnsignedPartialArithmeticTests[ULong](h)
    _CommonPartialArithmeticTests[USize](h)?
    _UnsignedPartialArithmeticTests[USize](h)

class iso _TestNextPow2 is UnitTest
  """
  Test power of 2 computations.
  """
  fun name(): String => "builtin/NextPow2"

  fun apply(h: TestHelper) =>
    h.assert_eq[U8](32, U8(17).next_pow2())
    h.assert_eq[U8](16, U8(16).next_pow2())
    h.assert_eq[U8](1, U8(0).next_pow2())
    h.assert_eq[U8](1, U8.max_value().next_pow2())

    h.assert_eq[U16](32, U16(17).next_pow2())
    h.assert_eq[U16](16, U16(16).next_pow2())
    h.assert_eq[U16](1, U16(0).next_pow2())
    h.assert_eq[U16](1, U16.max_value().next_pow2())

    h.assert_eq[U32](32, U32(17).next_pow2())
    h.assert_eq[U32](16, U32(16).next_pow2())
    h.assert_eq[U32](1, U32(0).next_pow2())
    h.assert_eq[U32](1, U32.max_value().next_pow2())

    h.assert_eq[U64](32, U64(17).next_pow2())
    h.assert_eq[U64](16, U64(16).next_pow2())
    h.assert_eq[U64](1, U64(0).next_pow2())
    h.assert_eq[U64](1, U64.max_value().next_pow2())

    h.assert_eq[ULong](32, ULong(17).next_pow2())
    h.assert_eq[ULong](16, ULong(16).next_pow2())
    h.assert_eq[ULong](1, ULong(0).next_pow2())
    h.assert_eq[ULong](1, ULong.max_value().next_pow2())

    h.assert_eq[USize](32, USize(17).next_pow2())
    h.assert_eq[USize](16, USize(16).next_pow2())
    h.assert_eq[USize](1, USize(0).next_pow2())
    h.assert_eq[USize](1, USize.max_value().next_pow2())

    h.assert_eq[U128](32, U128(17).next_pow2())
    h.assert_eq[U128](16, U128(16).next_pow2())
    h.assert_eq[U128](1, U128(0).next_pow2())
    h.assert_eq[U128](1, U128.max_value().next_pow2())

class iso _TestNumberConversionSaturation is UnitTest
  """
  Test saturation semantics for floating point/integer conversions.
  """
  fun name(): String => "builtin/NumberConversionSaturation"

  fun apply(h: TestHelper) =>
    float_to_ints[F32](h)
    float_to_ints[F64](h)
    float_to_int[F64, U128](h)

    h.assert_eq[U128](0, F32.min_value().u128())
    h.assert_eq[U128](U128.max_value(), (F32(1) / 0).u128())
    h.assert_eq[U128](0, (F32(-1) / 0).u128())

    h.assert_eq[F32](F32(1) / 0, U128.max_value().f32())

    h.assert_eq[F32](F32(1) / 0, F64.max_value().f32())
    h.assert_eq[F32](-F32(1) / 0, F64.min_value().f32())
    h.assert_eq[F32](F32(1) / 0, (F64(1) / 0).f32())
    h.assert_eq[F32](-F32(1) / 0, (-F64(1) / 0).f32())

  fun float_to_ints[A: (Float & Real[A])](h: TestHelper) =>
    float_to_int[A, I8](h)
    float_to_int[A, I16](h)
    float_to_int[A, I32](h)
    float_to_int[A, I64](h)
    float_to_int[A, ILong](h)
    float_to_int[A, ISize](h)
    float_to_int[A, I128](h)

    float_to_int[A, U8](h)
    float_to_int[A, U16](h)
    float_to_int[A, U32](h)
    float_to_int[A, U64](h)
    float_to_int[A, ULong](h)
    float_to_int[A, USize](h)

  fun float_to_int[A: (Float & Real[A]), B: (Number & Real[B])](
    h: TestHelper)
  =>
    h.assert_eq[B](B.max_value(), B.from[A](A.max_value()))
    h.assert_eq[B](B.min_value(), B.from[A](A.min_value()))

    let inf = A.from[I8](1) / A.from[I8](0)
    h.assert_eq[B](B.max_value(), B.from[A](inf))
    h.assert_eq[B](B.min_value(), B.from[A](-inf))

struct _TestStruct
  var i: U32 = 0
  new create() => None

class iso _TestNullablePointer is UnitTest
  """
  Test the NullablePointer type.
  """
  fun name(): String => "builtin/NullablePointer"

  fun apply(h: TestHelper) ? =>
    let a = NullablePointer[_TestStruct].none()
    h.assert_true(a.is_none())

    h.assert_error({() ? => let from_a = a()? })

    let s = _TestStruct
    s.i = 7

    let b = NullablePointer[_TestStruct](s)
    h.assert_false(b.is_none())

    let from_b = b()?
    h.assert_eq[U32](s.i, from_b.i)

class iso _TestLambdaCapture is UnitTest
  """
  Test free variable capture in lambdas.
  """
  fun name(): String => "builtin/LambdaCapture"

  fun apply(h: TestHelper) =>
    let x = "hi"
    let f = {(y: String): String => x + y}
    h.assert_eq[String]("hi there", f(" there"))
