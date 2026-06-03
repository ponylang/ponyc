use @pony_exitcode[None](code: I32)

primitive NegDefault
  fun apply(prec: USize = -1): USize => prec

primitive BinopDefault
  fun apply(x: USize = 0 + 1): USize => x

primitive GenericNegDefault
  fun apply[A: Stringable val](x: USize = -1): USize => x

class val LiteralDefaultConstructor
  let value: USize
  new val create(prec: USize = -1) =>
    value = prec

actor Main
  new create(env: Env) =>
    // Partial application of a method whose default argument desugars to a
    // call on a literal (`-1` -> `(1).neg()`) used to crash the compiler in
    // `lookup_base` (issue #2480). Call the partial application so the default
    // is actually applied.
    let f = NegDefault~apply()
    let a = f()

    // A default that is a literal binary operator (`0 + 1` -> `(0).add(1)`).
    let g = BinopDefault~apply()
    let b = g()

    // A generic method with a literal-operator default, partially applied with
    // an explicit type argument, then called.
    let h = GenericNegDefault~apply[String]()
    let c = h()

    // Partial application of a constructor with a literal-operator default.
    let i = LiteralDefaultConstructor~create()
    let d = i().value

    if (a == USize.max_value()) and (b == 1) and (c == USize.max_value())
      and (d == USize.max_value())
    then
      @pony_exitcode(42)
    end
