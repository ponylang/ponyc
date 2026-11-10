class Accumulator
  """
  Sums U64 values. Used by the mutation-runtime test to exercise VCR
  on a void-returning method call.
  """
  var _total: U64 = 0

  fun ref add(n: U64) =>
    _total = _total + n

  fun ref bump(): U64 =>
    _total = _total + 1
    _total

  fun total(): U64 =>
    _total

class _BorHelper
  var ran: Bool = false

  fun ref f(v: Bool): Bool =>
    ran = true
    v

  fun ref g(v: Bool): Bool ? =>
    ran = true
    error

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

    // Exercise VCR side-effect bypass: the argument expression has a side
    // effect (bump increments acc2). When VCR fires, the argument must not
    // be evaluated, so acc2.total() stays 0.
    let acc2 = Accumulator
    acc2.add(acc2.bump())
    env.out.print("vcr_se=" + acc2.total().string())
    if acc2.total() != U64(2) then
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
    env.out.print("cr_arith=" + cr_arith.string())
    if cr_arith != U64(13) then
      env.exitcode(1)
    end

    // CR divrem: literal 3 in a % 3.
    let cr_rem: U64 = a % 3  // 3 % 3 = 0
    env.out.print("cr_rem=" + cr_rem.string())
    if cr_rem != U64(0) then
      env.exitcode(1)
    end

    // CR shift: literal 3 in a << 3.
    let cr_shift: U64 = a << 3  // 3 << 3 = 24
    env.out.print("cr_shift=" + cr_shift.string())
    if cr_shift != U64(24) then
      env.exitcode(1)
    end

    // CR bitwise: literal 0x07 in a and 0x07.
    let cr_bit: U64 = a and 0x07  // 3 and 7 = 3
    env.out.print("cr_bit=" + cr_bit.string())
    if cr_bit != U64(3) then
      env.exitcode(1)
    end

    // CR neg: literal 7 in -7.
    let cr_neg: I64 = -7  // -7
    env.out.print("cr_neg=" + cr_neg.string())
    if cr_neg != I64(-7) then
      env.exitcode(1)
    end

    // CR not: literal 0x0F in (a and (not 0x0F)).
    let cr_not: U64 = a and (not 0x0F)  // 3 and ~15 = 0
    env.out.print("cr_not=" + cr_not.string())
    if cr_not != U64(0) then
      env.exitcode(1)
    end

    // CR U8 boundary: literal 255 in y + 255 on U8.
    // n+1 wraps to 0 via LLVMConstInt truncation (256 → 0 for i8).
    let y_u8: U8 = U8(1)
    let cr_u8: U8 = y_u8 + 255  // 1 + 255 = 0 (wraps)
    env.out.print("cr_u8=" + cr_u8.string())
    if cr_u8 != U8(0) then
      env.exitcode(1)
    end

    // CR wide literal: U128 above 2^64. Variable operand prevents Rule 2.
    let small: U128 = a.u128()  // 3
    let cr_wide: U128 = small + 0x1_0000_0000_0000_000A  // 3 + 2^64+10
    env.out.print("cr_wide=" + cr_wide.string())
    if cr_wide != (small + U128(0x1_0000_0000_0000_000A)) then
      env.exitcode(1)
    end

    let p: U64 = 2
    let q: U64 = 3
    let atrue: Bool = p < q
    let afalse: Bool = p > q

    // Row 1: a and f() with a=true, f returns false.
    // Unmutated: true and false = false, ran=true.
    // and->or: left true, skip right, result true, ran=false.
    let h1 = _BorHelper
    let r1 = atrue and h1.f(false)
    env.out.print("bor1=" + r1.string() + "," + h1.ran.string())
    if r1 != false then env.exitcode(1) end

    // Row 2: a and f() with a=false, f returns true.
    // Unmutated: false and (skip) = false, ran=false.
    // and->or: left false, enter right, result true, ran=true.
    let h2 = _BorHelper
    let r2 = afalse and h2.f(true)
    env.out.print("bor2=" + r2.string() + "," + h2.ran.string())
    if r2 != false then env.exitcode(1) end

    // Row 3: a or f() with a=true, f returns false.
    // Unmutated: true or (skip) = true, ran=false.
    // or->and: left true, enter right, result false, ran=true.
    let h3 = _BorHelper
    let r3 = atrue or h3.f(false)
    env.out.print("bor3=" + r3.string() + "," + h3.ran.string())
    if r3 != true then env.exitcode(1) end

    // Row 4: a or f() with a=false, f returns true.
    // Unmutated: false or true = true, ran=true.
    // or->and: left false, skip right, result false, ran=false.
    let h4 = _BorHelper
    let r4 = afalse or h4.f(true)
    env.out.print("bor4=" + r4.string() + "," + h4.ran.string())
    if r4 != true then env.exitcode(1) end

    // Row 5: (p < q) and true where p<q is false at runtime (p=2,q=3
    // so p<q=true; use p>q=false instead).
    // Unmutated: false and true = false. and->or: false, skip, true.
    let r5 = (p > q) and true
    env.out.print("bor5=" + r5.string())
    if r5 != false then env.exitcode(1) end

    // Row 6: a xor f() with a=false, f returns true.
    // Unmutated: false xor true = true, ran=true.
    // xor->and: a=false, skip right, result false, ran=false.
    let h6 = _BorHelper
    let r6 = afalse xor h6.f(true)
    env.out.print("bor6=" + r6.string() + "," + h6.ran.string())
    if r6 != true then env.exitcode(1) end

    // Row 7: a xor f() with a=true, f returns true.
    // Unmutated: true xor true = false, ran=true.
    // xor->and: a=true, enter right, result true, ran=true.
    let h7 = _BorHelper
    let r7 = atrue xor h7.f(true)
    env.out.print("bor7=" + r7.string() + "," + h7.ran.string())
    if r7 != false then env.exitcode(1) end

    // Row 8: a xor f() with a=true, f returns true.
    // Unmutated: true xor true = false, ran=true.
    // xor->or: a=true, skip right, result true, ran=false.
    let h8 = _BorHelper
    let r8 = atrue xor h8.f(true)
    env.out.print("bor8=" + r8.string() + "," + h8.ran.string())
    if r8 != false then env.exitcode(1) end

    // Row 9: try a xor g()? else false end with a=false.
    // Unmutated: false xor (error) → false, ran=true.
    // xor->and: a=false, skip right, result false, ran=false.
    let h9 = _BorHelper
    let r9 = try afalse xor h9.g(true)? else false end
    env.out.print("bor9=" + r9.string() + "," + h9.ran.string())
    if r9 != false then env.exitcode(1) end

  fun _compute(x: U64, y: U64): U64 =>
    x + y

  fun _never_called(env: Env) =>
    let x: U64 = 10
    let y: U64 = 20
    let z = x + y
    env.exitcode(z.i32())
