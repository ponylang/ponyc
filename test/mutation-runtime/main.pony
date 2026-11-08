class Accumulator
  """
  Sums U64 values. Used by the mutation-runtime test to exercise VCR
  on a void-returning method call.
  """
  var _total: U64 = 0

  fun ref add(n: U64) =>
    _total = _total + n

  fun total(): U64 =>
    _total

actor Main
  new create(env: Env) =>
    let a: U64 = 3
    let b: U64 = 2
    let result = a + b
    if result == U64(5) then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
    // Make _never_called reachable to the compiler but unreachable at
    // runtime: result is always 5, so this branch is never taken.
    if result == U64(0) then
      _never_called(env)
    end

    // Exercise RVR: a function returning a numeric value.
    if _compute(a, b) != U64(5) then
      env.exitcode(1)
    end

    // Exercise AMR: wrapping arithmetic that overflows.
    let w: U8 = U8(200) +~ U8(100)
    if w != U8(44) then
      env.exitcode(1)
    end

    // Exercise VCR: a void-returning method call.
    let acc = Accumulator
    acc.add(a)
    if acc.total() != a then
      env.exitcode(1)
    end

    // Exercise CR: a literal in a comparison.
    // result (5) == 5 is true; CR mutant 5->6 or 5->4 makes it false.
    if result == 5 then
      None
    else
      env.exitcode(1)
    end

    // Exercise SOR: shift operator.
    let shifted: U64 = a << b  // 3 << 2 = 12
    if shifted != U64(12) then
      env.exitcode(1)
    end

    // Exercise BOR: bitwise operator.
    let anded: U64 = U64(0xFF) and U64(0x0F)  // 15
    if anded != U64(15) then
      env.exitcode(1)
    end

    // Exercise ROR: comparison operator.
    if not (a > b) then  // 3 > 2 is true
      env.exitcode(1)
    end

    // Exercise UOI: unary neg.
    let neg_val: I64 = -I64(5)  // -5
    if neg_val != I64(-5) then
      env.exitcode(1)
    end

    // CR arithmetic: literal 10 in a + 10.
    let cr_arith: U64 = a + 10  // 3 + 10 = 13
    if cr_arith != U64(13) then
      env.exitcode(1)
    end

    // CR divrem: literal 3 in a % 3.
    let cr_rem: U64 = a % 3  // 3 % 3 = 0
    if cr_rem != U64(0) then
      env.exitcode(1)
    end

    // CR shift: literal 3 in a << 3.
    let cr_shift: U64 = a << 3  // 3 << 3 = 24
    if cr_shift != U64(24) then
      env.exitcode(1)
    end

    // CR bitwise: literal 0x07 in a and 0x07.
    let cr_bit: U64 = a and 0x07  // 3 and 7 = 3
    if cr_bit != U64(3) then
      env.exitcode(1)
    end

    // CR neg: literal 7 in -7.
    let cr_neg: I64 = -7  // -7
    if cr_neg != I64(-7) then
      env.exitcode(1)
    end

    // CR not: literal 0x0F in (a and (not 0x0F)).
    let cr_not: U64 = a and (not 0x0F)  // 3 and ~15 = 0
    if cr_not != U64(0) then
      env.exitcode(1)
    end

    // CR wide literal: U128 above 2^64. Variable operand prevents Rule 2.
    let small: U128 = a.u128()  // 3
    let cr_wide: U128 = small + 0x1_0000_0000_0000_000A  // 3 + 2^64+10
    if cr_wide != (small + U128(0x1_0000_0000_0000_000A)) then
      env.exitcode(1)
    end

  fun _compute(x: U64, y: U64): U64 =>
    x + y

  fun _never_called(env: Env) =>
    let x: U64 = 10
    let y: U64 = 20
    let z = x + y
    env.exitcode(z.i32())
