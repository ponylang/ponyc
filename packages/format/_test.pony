use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestApply)
    test(_TestInt)

class iso _TestApply is UnitTest
  fun name(): String => "format/apply"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("fmt      ",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9))
    h.assert_eq[String]("      fmt",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9, AlignRight))
    h.assert_eq[String]("   fmt   ",
      Format("fmt", FormatDefault, PrefixDefault, -1, 9, AlignCenter))

class iso _TestInt is UnitTest
  fun name(): String => "format/int"

  fun apply(h: TestHelper) =>
    ifdef ilp32 or llp64 then
      h.assert_eq[String]("-2147483648",
        Format.int[ILong](ILong.min_value()))
    else
      h.assert_eq[String]("-9223372036854775808",
        Format.int[ILong](ILong.min_value()))
    end
    ifdef ilp32 then
      h.assert_eq[String]("-2147483648",
        Format.int[ISize](ISize.min_value()))
    else
      h.assert_eq[String]("-9223372036854775808",
        Format.int[ISize](ISize.min_value()))
    end
    h.assert_eq[String]("-9223372036854775808",
      Format.int[I64](I64.min_value()))
    h.assert_eq[String]("0",
      Format.int[U32](U32.min_value()))
    h.assert_eq[String]("-2147483648",
      Format.int[I32](I32.min_value()))
    h.assert_eq[String]("-32768",
      Format.int[I16](I16.min_value()))
    h.assert_eq[String]("-128",
      Format.int[I8](I8.min_value()))
    h.assert_eq[String]("00010",
      Format.int[U64](10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String]("0x0000A",
      Format.int[U64](10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String]("0000A",
      Format.int[U64](10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String]("0x0000a",
      Format.int[U64](10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String]("0000a",
      Format.int[U64](10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](" 0x0000A",
      Format.int[U64](10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String]("0x0000A ",
      Format.int[U64](10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String]("   0000a",
      Format.int[U64](10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String]("0000a   ",
      Format.int[U64](10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String]("-00010",
      Format.int[I64](-10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String]("-0000A",
      Format.int[I64](-10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000a",
      Format.int[I64](-10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String]("-0000a",
      Format.int[I64](-10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String]("-0x0000A",
      Format.int[I64](-10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String]("  -0000a",
      Format.int[I64](-10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String]("-0000a  ",
      Format.int[I64](-10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String]("00010",
      Format.int[U128](10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String]("0x0000A",
      Format.int[U128](10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String]("0000A",
      Format.int[U128](10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String]("0x0000a",
      Format.int[U128](10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String]("0000a",
      Format.int[U128](10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String](" 0x0000A",
      Format.int[U128](10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String]("0x0000A ",
      Format.int[U128](10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String]("   0000a",
      Format.int[U128](10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String]("0000a   ",
      Format.int[U128](10, FormatHexSmallBare, PrefixDefault, 5, 8, AlignLeft))

    h.assert_eq[String]("-00010",
      Format.int[I128](-10, FormatDefault, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5))
    h.assert_eq[String]("-0000A",
      Format.int[I128](-10, FormatHexBare, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000a",
      Format.int[I128](-10, FormatHexSmall, PrefixDefault, 5))
    h.assert_eq[String]("-0000a",
      Format.int[I128](-10, FormatHexSmallBare, PrefixDefault, 5))
    h.assert_eq[String]("-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5, 8))
    h.assert_eq[String]("-0x0000A",
      Format.int[I128](-10, FormatHex, PrefixDefault, 5, 8, AlignLeft))
    h.assert_eq[String]("  -0000a",
      Format.int[I128](-10, FormatHexSmallBare, PrefixDefault, 5, 8))
    h.assert_eq[String]("-0000a  ",
      Format.int[I128](-10, FormatHexSmallBare, PrefixDefault, 5, 8,
        AlignLeft))
