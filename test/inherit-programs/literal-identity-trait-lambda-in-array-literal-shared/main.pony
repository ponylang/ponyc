// T.g takes a lambda out of an array literal. Expects C1.g() and C2.g() to be
// the same object: both classes share the lambda's class.
trait T
  fun g(): {(U64): U64} val ? =>
    let a = [as {(U64): U64} val: {(x) => x}]
    a(0)?
class C1 is T
class C2 is T
actor Main
  new create(env: Env) =>
    try env.out.print((C1.g()? is C2.g()?).string() + " " + (C1.g()? is C1.g()?).string()) end
