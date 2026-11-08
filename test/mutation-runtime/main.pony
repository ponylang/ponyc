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

  fun _compute(x: U64, y: U64): U64 =>
    x + y

  fun _never_called(env: Env) =>
    let x: U64 = 10
    let y: U64 = 20
    let z = x + y
    env.exitcode(z.i32())
