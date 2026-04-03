"""
Regression test for issue #4507: Runtime crash when pattern matching
a tuple against a union type.

This test verifies that matching tuple elements against union types works
correctly for all primitive numeric types, including edge cases like
minimum/maximum values and types requiring special alignment (I128/U128).
"""

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    // Test all unsigned integer types
    test_u8()
    test_u16()
    test_u32()
    test_u64()
    test_u128()

    // Test all signed integer types
    test_i8()
    test_i16()
    test_i32()
    test_i64()
    test_i128()

    // Test floating point types
    test_f32()
    test_f64()

    // Test edge cases with unions
    test_union_combinations()

    // Test nested tuples with unions
    test_nested_tuples()

    // Test non-matching cases
    test_non_matching()

  fun ref fail(msg: String) =>
    _env.out.print("FAIL: " + msg)
    _env.exitcode(1)

  fun ref test_u8() =>
    // Test U8 with various values
    let cases: Array[(String, U8)] val = [
      ("U8 zero", U8(0))
      ("U8 one", U8(1))
      ("U8 mid", U8(128))
      ("U8 max", U8.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (U8 | U16)) =>
        match v
        | let actual: U8 if actual == expected => None
        | let actual: U8 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_u16() =>
    let cases: Array[(String, U16)] val = [
      ("U16 zero", U16(0))
      ("U16 one", U16(1))
      ("U16 mid", U16(32768))
      ("U16 max", U16.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (U8 | U16)) =>
        match v
        | let actual: U16 if actual == expected => None
        | let actual: U16 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_u32() =>
    let cases: Array[(String, U32)] val = [
      ("U32 zero", U32(0))
      ("U32 one", U32(1))
      ("U32 mid", U32(2147483648))
      ("U32 max", U32.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (U32 | U64)) =>
        match v
        | let actual: U32 if actual == expected => None
        | let actual: U32 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_u64() =>
    let cases: Array[(String, U64)] val = [
      ("U64 zero", U64(0))
      ("U64 one", U64(1))
      ("U64 large", U64(9223372036854775808))
      ("U64 max", U64.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (U32 | U64)) =>
        match v
        | let actual: U64 if actual == expected => None
        | let actual: U64 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_u128() =>
    // U128 requires 16-byte alignment - this is a critical test case
    let cases: Array[(String, U128)] val = [
      ("U128 zero", U128(0))
      ("U128 one", U128(1))
      ("U128 fits_in_64", U128(9999999999999999999))
      ("U128 large", U128(170141183460469231731687303715884105727))
      ("U128 max", U128.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (U64 | U128)) =>
        match v
        | let actual: U128 if actual == expected => None
        | let actual: U128 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_i8() =>
    let cases: Array[(String, I8)] val = [
      ("I8 zero", I8(0))
      ("I8 one", I8(1))
      ("I8 neg_one", I8(-1))
      ("I8 min", I8.min_value())
      ("I8 max", I8.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (I8 | I16)) =>
        match v
        | let actual: I8 if actual == expected => None
        | let actual: I8 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_i16() =>
    let cases: Array[(String, I16)] val = [
      ("I16 zero", I16(0))
      ("I16 one", I16(1))
      ("I16 neg_one", I16(-1))
      ("I16 min", I16.min_value())
      ("I16 max", I16.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (I8 | I16)) =>
        match v
        | let actual: I16 if actual == expected => None
        | let actual: I16 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_i32() =>
    let cases: Array[(String, I32)] val = [
      ("I32 zero", I32(0))
      ("I32 one", I32(1))
      ("I32 neg_one", I32(-1))
      ("I32 min", I32.min_value())
      ("I32 max", I32.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (I32 | I64)) =>
        match v
        | let actual: I32 if actual == expected => None
        | let actual: I32 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_i64() =>
    let cases: Array[(String, I64)] val = [
      ("I64 zero", I64(0))
      ("I64 one", I64(1))
      ("I64 neg_one", I64(-1))
      ("I64 min", I64.min_value())
      ("I64 max", I64.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (I32 | I64)) =>
        match v
        | let actual: I64 if actual == expected => None
        | let actual: I64 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_i128() =>
    // I128 requires 16-byte alignment - this is a critical test case
    let cases: Array[(String, I128)] val = [
      ("I128 zero", I128(0))
      ("I128 one", I128(1))
      ("I128 neg_one", I128(-1))
      ("I128 large_pos", I128(170141183460469231731687303715884105727))
      ("I128 large_neg", I128(-170141183460469231731687303715884105727))
      ("I128 min", I128.min_value())
      ("I128 max", I128.max_value())
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (I64 | I128)) =>
        match v
        | let actual: I128 if actual == expected => None
        | let actual: I128 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_f32() =>
    let cases: Array[(String, F32)] val = [
      ("F32 zero", F32(0.0))
      ("F32 one", F32(1.0))
      ("F32 neg_one", F32(-1.0))
      ("F32 pi", F32(3.14159))
      ("F32 small", F32(1.17549435e-38))
      ("F32 large", F32(3.40282347e+38))
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (F32 | F64)) =>
        match v
        | let actual: F32 if actual == expected => None
        | let actual: F32 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_f64() =>
    let cases: Array[(String, F64)] val = [
      ("F64 zero", F64(0.0))
      ("F64 one", F64(1.0))
      ("F64 neg_one", F64(-1.0))
      ("F64 pi", F64(3.141592653589793))
      ("F64 small", F64(2.2250738585072014e-308))
      ("F64 large", F64(1.7976931348623157e+308))
    ]

    for (name, expected) in cases.values() do
      let x: Any val = ("test", expected)
      match x
      | (let s: String, let v: (F32 | F64)) =>
        match v
        | let actual: F64 if actual == expected => None
        | let actual: F64 => fail(name + ": got " + actual.string() + ", expected " + expected.string())
        else
          fail(name + ": wrong type extracted from union")
        end
      else
        fail(name + ": tuple match failed")
      end
    end

  fun ref test_union_combinations() =>
    // Test with larger unions
    let x1: Any val = ("test", U8(42))
    match x1
    | (let s: String, let v: (U8 | U16 | U32 | U64)) =>
      match v
      | let actual: U8 if actual == 42 => None
      else
        fail("union_combo_u8: wrong value or type")
      end
    else
      fail("union_combo_u8: tuple match failed")
    end

    let x2: Any val = ("test", U64(999999999999))
    match x2
    | (let s: String, let v: (U8 | U16 | U32 | U64)) =>
      match v
      | let actual: U64 if actual == 999999999999 => None
      else
        fail("union_combo_u64: wrong value or type")
      end
    else
      fail("union_combo_u64: tuple match failed")
    end

    // Test union with 128-bit type in larger union
    let x3: Any val = ("test", I128(-12345678901234567890))
    match x3
    | (let s: String, let v: (I8 | I16 | I32 | I64 | I128)) =>
      match v
      | let actual: I128 if actual == -12345678901234567890 => None
      else
        fail("union_combo_i128: wrong value or type")
      end
    else
      fail("union_combo_i128: tuple match failed")
    end

  fun ref test_nested_tuples() =>
    // Test tuple with union in different positions
    let x1: Any val = (U8(10), "middle", U128(99999999999999999999))
    match x1
    | (let a: (U8 | U16), let b: String, let c: (U64 | U128)) =>
      var ok = true
      match a
      | let av: U8 if av == 10 => None
      else
        ok = false
      end
      match c
      | let cv: U128 if cv == 99999999999999999999 => None
      else
        ok = false
      end
      if not ok then
        fail("nested_tuple_1: wrong values")
      end
    else
      fail("nested_tuple_1: match failed")
    end

    // Test deeply nested - use tuple pattern directly
    let x2: Any val = (("inner", I128(777)), "outer")
    match x2
    | ((let s: String, let v: (I64 | I128)), let outer: String) =>
      match v
      | let actual: I128 if actual == 777 => None
      else
        fail("nested_tuple_2: wrong inner value")
      end
    else
      fail("nested_tuple_2: outer match failed")
    end

  fun ref test_non_matching() =>
    // Verify that non-matching types correctly don't match
    let x1: Any val = ("test", I64(42))
    match x1
    | (let s: String, let v: (U8 | U16)) =>
      fail("non_match_1: should not have matched I64 to (U8 | U16)")
    else
      None // Expected path
    end

    // U128 should not match I128
    let x2: Any val = ("test", U128(123))
    match x2
    | (let s: String, let v: (I64 | I128)) =>
      fail("non_match_2: should not have matched U128 to (I64 | I128)")
    else
      None // Expected path
    end

    // Wrong tuple cardinality
    let x3: Any val = ("test", U8(1), U8(2))
    match x3
    | (let s: String, let v: (U8 | U16)) =>
      fail("non_match_3: should not have matched 3-tuple to 2-tuple pattern")
    else
      None // Expected path
    end
