// T.g passes a lambda to app, so the lambda's type comes from the call
// argument. Expects C.g() and D.g() to be the same object: C's and D's copies
// build equal lambda classes, which are shared.
trait T
  fun app(f: {(U64): U64} val): {(U64): U64} val => f
  fun g(): {(U64): U64} val => app({(x) => x + 1})
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C.g() is D.g()).string() + " " + (C.g() is C.g()).string())
