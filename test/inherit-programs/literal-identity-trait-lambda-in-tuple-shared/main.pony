// T.g puts a lambda in a tuple and returns it. Expects C1.g() and C2.g() to be
// the same object: both classes share the lambda's class.
trait T
  fun g(): {(U64): U64} val =>
    let t: ({(U64): U64} val, U8) = ({(x) => x}, 1)
    t._1
class C1 is T
class C2 is T
actor Main
  new create(env: Env) =>
    env.out.print((C1.g() is C2.g()).string() + " " + (C1.g() is C1.g()).string())
