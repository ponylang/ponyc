use "pony_test"
use "collections"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestApply)
    test(_TestFloat)
    test(_TestInt)
    test(_TestIntUTF32)

class \nodoc\ iso _TestApply is UnitTest
  fun name(): String => "format/apply"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      "fmt      ",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9))
    h.assert_eq[String](
      "      fmt",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9, AlignRight))
    h.assert_eq[String](
      "   fmt   ",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9, AlignCenter))

class \nodoc\ iso _TestFloat is UnitTest
  fun name(): String => "format/float"

  fun apply(h: TestHelper) =>
    // FormatFix with a large exponent needs hundreds of digits for the integer
    // part.
    let large: String val =
      Format.float[F64](1e300 where fmt = FormatFix, prec = 0)
    h.assert_eq[USize](301, large.size())
    h.assert_true(large.at("1000000000000000", 0))

    let large_neg: String val =
      Format.float[F64](-1e300 where fmt = FormatFix, prec = 0)
    h.assert_eq[USize](302, large_neg.size())
    h.assert_true(large_neg.at("-1000000000000000", 0))

    // Default precision adds six decimal digits
    let large_prec: String val =
      Format.float[F64](1e300 where fmt = FormatFix)
    h.assert_eq[USize](308, large_prec.size())
    h.assert_true(large_prec.at(".000000", large_prec.size().isize() - 7))

    // Normal values
    h.assert_eq[String](
      "3.140000",
      Format.float[F64](3.14 where fmt = FormatFix))
    h.assert_eq[String](
      "0.000000",
      Format.float[F64](0.0 where fmt = FormatFix))

    // FormatExp is unaffected (compact output) but verify it works
    h.assert_eq[String](
      "1.000000e+300",
      Format.float[F64](1e300 where fmt = FormatExp))

    // FormatGeneral
    h.assert_eq[String](
      "1e+300",
      Format.float[F64](1e300 where fmt = FormatGeneral))

class \nodoc\ iso _TestInt is UnitTest
  fun name(): String => "format/int"

  fun apply(h: TestHelper) =>
    ifdef ilp32 or llp64 then
      h.assert_eq[String](
        "-2147483648",
        Format.int[ILong](ILong.min_value()))
    else
      h.assert_eq[String](
        "-9223372036854775808",
        Format.int[ILong](ILong.min_value()))
    end
    ifdef ilp32 then
      h.assert_eq[String](
        "-2147483648",
        Format.int[ISize](ISize.min_value()))
    else
      h.assert_eq[String](
        "-9223372036854775808",
        Format.int[ISize](ISize.min_value()))
    end
    h.assert_eq[String](
      "-9223372036854775808",
      Format.int[I64](I64.min_value()))
    h.assert_eq[String](
      "0",
      Format.int[U32](U32.min_value()))
    h.assert_eq[String](
      "-2147483648",
      Format.int[I32](I32.min_value()))
    h.assert_eq[String](
      "-32768",
      Format.int[I16](I16.min_value()))
    h.assert_eq[String](
      "-128",
      Format.int[I8](I8.min_value()))
    h.assert_eq[String](
      "00010",
      Format.int[U64](10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String](
      "0x0000A",
      Format.int[U64](10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String](
      "0000A",
      Format.int[U64](10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String](
      "0x0000a",
      Format.int[U64](10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String](
      "0000a",
      Format.int[U64](10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](
      " 0x0000A",
      Format.int[U64](10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "0x0000A ",
      Format.int[U64](10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String](
      "   0000a",
      Format.int[U64](10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "0000a   ",
      Format.int[U64](
        10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String](
      "-00010",
      Format.int[I64](-10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String](
      "-0000A",
      Format.int[I64](-10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000a",
      Format.int[I64](-10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String](
      "-0000a",
      Format.int[I64](-10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String](
      "  -0000a",
      Format.int[I64](-10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "-0000a  ",
      Format.int[I64](
        -10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String](
      "00010",
      Format.int[U128](10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String](
      "0x0000A",
      Format.int[U128](10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String](
      "0000A",
      Format.int[U128](10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String](
      "0x0000a",
      Format.int[U128](10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String](
      "0000a",
      Format.int[U128](10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](
      " 0x0000A",
      Format.int[U128](10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "0x0000A ",
      Format.int[U128](10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String](
      "   0000a",
      Format.int[U128](10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "0000a   ",
      Format.int[U128](
        10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String](
      "-00010",
      Format.int[I128](-10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String](
      "-0000A",
      Format.int[I128](-10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000a",
      Format.int[I128](-10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String](
      "-0000a",
      Format.int[I128](-10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String](
      "  -0000a",
      Format.int[I128](-10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String](
      "-0000a  ",
      Format.int[I128](
        -10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

class \nodoc\ iso _TestIntUTF32 is UnitTest
  fun name(): String => "format/int_utf32"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("A", Format.int[U8](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[U16](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[U32](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[U64](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[U128](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[I8](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[I16](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[I32](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[I64](65, FormatUTF32))
    h.assert_eq[String]("A", Format.int[I128](65, FormatUTF32))

    // U+00E9 (2-byte UTF-8), fits in U8
    h.assert_eq[String]("é", Format.int[U8](0xE9, FormatUTF32))
    // U+263A (3-byte UTF-8), fits in U16
    h.assert_eq[String]("☺", Format.int[U16](0x263A, FormatUTF32))
