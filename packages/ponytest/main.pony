use "collections"

actor Main
  new create(env: Env) =>
    var test = PonyTest(env)

    test(recover _TestStringRunes end)
    test(recover _TestIntToString end)
    test(recover _TestStringToU8 end)
    test(recover _TestStringToI8 end)
    test(recover _TestStringToIntLarge end)
    test(recover _TestSpecialValuesF32 end)
    test(recover _TestSpecialValuesF64 end)

    test.complete()


class _TestStringRunes iso is UnitTest
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


class _TestIntToString iso is UnitTest
  """
  Test converting integers to strings.
  """
  fun name(): String => "tostring.int"

  fun apply(h: TestHelper): TestResult =>
    h.expect_eq[String]("0", U32(0).string())
    h.expect_eq[String]("3", U32(3).string())
    h.expect_eq[String]("1234", U32(1234).string())
    h.expect_eq[String]("003", U32(3).string(where prec = 3))
    h.expect_eq[String]("  3", U32(3).string(where width = 3))
    h.expect_eq[String]("  003", U32(3).string(where prec = 3, width = 5))

    true


class _TestStringToU8 iso is UnitTest
  """
  Test converting strings to U8s.
  """
  fun name(): String => "parsestring.U8"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[U8](0, "0".u8())
    h.expect_eq[U8](123, "123".u8())
    h.expect_eq[U8](123, "0123".u8())
    h.expect_eq[U8](89, "089".u8())

    h.expect_error(object fun apply()? => "300".u8() end, "U8 300")
    h.expect_error(object fun apply()? => "30L".u8() end, "U8 30L")
    h.expect_error(object fun apply()? => "-10".u8() end, "U8 -10")
    
    h.expect_eq[U8](16, "0x10".u8())
    h.expect_eq[U8](31, "0x1F".u8())
    h.expect_eq[U8](31, "0x1f".u8())
    h.expect_eq[U8](31, "0X1F".u8())
    h.expect_eq[U8](2, "0b10".u8())
    h.expect_eq[U8](2, "0B10".u8())
    h.expect_eq[U8](0x8A, "0b1000_1010".u8())
    
    h.expect_error(object fun apply()? => "1F".u8() end, "U8 1F")
    h.expect_error(object fun apply()? => "0x".u8() end, "U8 0x")
    h.expect_error(object fun apply()? => "0b3".u8() end, "U8 0b3")
    h.expect_error(object fun apply()? => "0d4".u8() end, "U8 0d4")
    
    true


class _TestStringToI8 iso is UnitTest
  """
  Test converting strings to I8s.
  """
  fun name(): String => "parsestring.I8"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[I8](0, "0".i8())
    h.expect_eq[I8](123, "123".i8())
    h.expect_eq[I8](123, "0123".i8())
    h.expect_eq[I8](89, "089".i8())
    h.expect_eq[I8](-10, "-10".i8())

    h.expect_error(object fun apply()? => "200".i8() end, "I8 200")
    h.expect_error(object fun apply()? => "30L".i8() end, "I8 30L")
    
    h.expect_eq[I8](16, "0x10".i8())
    h.expect_eq[I8](31, "0x1F".i8())
    h.expect_eq[I8](31, "0x1f".i8())
    h.expect_eq[I8](31, "0X1F".i8())
    h.expect_eq[I8](2, "0b10".i8())
    h.expect_eq[I8](2, "0B10".i8())
    h.expect_eq[I8](0x4A, "0b100_1010".i8())
    h.expect_eq[I8](-0x4A, "-0b100_1010".i8())
    
    h.expect_error(object fun apply()? => "1F".i8() end, "U8 1F")
    h.expect_error(object fun apply()? => "0x".i8() end, "U8 0x")
    h.expect_error(object fun apply()? => "0b3".i8() end, "U8 0b3")
    h.expect_error(object fun apply()? => "0d4".i8() end, "U8 0d4")
    
    true


class _TestStringToIntLarge iso is UnitTest
  """
  Test converting strings to I* and U* types bigger than 8 bit.
  """
  fun name(): String => "parsestring.intlarge"

  fun apply(h: TestHelper): TestResult ? =>
    h.expect_eq[U16](0, "0".u16())
    h.expect_eq[U16](123, "123".u16())
    h.expect_error(object fun apply()? => "-10".u16() end, "U16 -10")
    h.expect_error(object fun apply()? => "65536".u16() end, "U16 65536")
    h.expect_error(object fun apply()? => "30L".u16() end, "U16 30L")
    
    h.expect_eq[I16](0, "0".i16())
    h.expect_eq[I16](123, "123".i16())
    h.expect_eq[I16](-10, "-10".i16())
    h.expect_error(object fun apply()? => "65536".i16() end, "I16 65536")
    h.expect_error(object fun apply()? => "30L".i16() end, "I16 30L")
    
    h.expect_eq[U32](0, "0".u32())
    h.expect_eq[U32](123, "123".u32())
    h.expect_error(object fun apply()? => "-10".u32() end, "U32 -10")
    h.expect_error(object fun apply()? => "30L".u32() end, "U32 30L")
    
    h.expect_eq[I32](0, "0".i32())
    h.expect_eq[I32](123, "123".i32())
    h.expect_eq[I32](-10, "-10".i32())
    h.expect_error(object fun apply()? => "30L".i32() end, "I32 30L")
    
    h.expect_eq[U64](0, "0".u64())
    h.expect_eq[U64](123, "123".u64())
    h.expect_error(object fun apply()? => "-10".u64() end, "U64 -10")
    h.expect_error(object fun apply()? => "30L".u64() end, "U64 30L")
    
    h.expect_eq[I64](0, "0".i64())
    h.expect_eq[I64](123, "123".i64())
    h.expect_eq[I64](-10, "-10".i64())
    h.expect_error(object fun apply()? => "30L".i64() end, "I64 30L")
    
    h.expect_eq[U128](0, "0".u128())
    h.expect_eq[U128](123, "123".u128())
    h.expect_error(object fun apply()? => "-10".u128() end, "U128 -10")
    h.expect_error(object fun apply()? => "30L".u128() end, "U128 30L")
    
    h.expect_eq[I128](0, "0".i128())
    h.expect_eq[I128](123, "123".i128())
    h.expect_eq[I128](-10, "-10".i128())
    h.expect_error(object fun apply()? => "30L".i128() end, "I128 30L")

    true


class _TestSpecialValuesF32 iso is UnitTest
  """
  Test whether a F32 is infinite or NaN.
  """
  fun name(): String => "specialvalue.F32"

  fun apply(h: TestHelper): TestResult =>
    // 1
    h.expect_true(F32(1.0).finite())
    h.expect_false(F32(1.0).nan())

    // -1
    h.expect_true(F32(-1.0).finite())
    h.expect_false(F32(-1.0).nan())

    // Infinity
    h.expect_false(F32(1.0 / 0.0).finite())
    h.expect_false(F32(1.0 / 0.0).nan())

    // - infinity
    h.expect_false(F32(-1.0 / 0.0).finite())
    h.expect_false(F32(-1.0 / 0.0).nan())
    
    // NaN
    h.expect_false(F32(0.0 / 0.0).finite())
    h.expect_true(F32(0.0 / 0.0).nan())

    true


class _TestSpecialValuesF64 iso is UnitTest
  """
  Test whether a F64 is infinite or NaN.
  """
  fun name(): String => "specialvalue.F64"

  fun apply(h: TestHelper): TestResult =>
    // 1
    h.expect_true(F64(1.0).finite())
    h.expect_false(F64(1.0).nan())

    // -1
    h.expect_true(F64(-1.0).finite())
    h.expect_false(F64(-1.0).nan())

    // Infinity
    h.expect_false(F64(1.0 / 0.0).finite())
    h.expect_false(F64(1.0 / 0.0).nan())

    // - infinity
    h.expect_false(F64(-1.0 / 0.0).finite())
    h.expect_false(F64(-1.0 / 0.0).nan())
    
    // NaN
    h.expect_false(F64(0.0 / 0.0).finite())
    h.expect_true(F64(0.0 / 0.0).nan())

    true
