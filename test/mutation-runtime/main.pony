class Accumulator
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

    // Exercise VCR: a void-returning method call.
    let acc = Accumulator
    acc.add(a)
    if acc.total() != a then
      env.exitcode(1)
    end

  fun _compute(x: U64, y: U64): U64 =>
    x + y

  fun _never_called(env: Env) =>
    let x: U64 = 10
    let y: U64 = 20
    let z = x + y
    env.exitcode(z.i32())
