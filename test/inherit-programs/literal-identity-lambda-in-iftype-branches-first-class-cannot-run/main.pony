// g's iftype keeps the lambda adding 1 for A = U8 and the one adding 2
// otherwise. Each class's copy of g makes classes for both lambdas, and a
// copy can share another copy's class. C2 gives U8 and C1 gives U16, so the
// test expects C2's g to add 1 and C1's to add 2, printing "2 3", whichever
// class is declared first.
trait T[A: Any val]
  fun app(f: {(U64): U64} val): {(U64): U64} val => f
  fun g(): {(U64): U64} val =>
    iftype A <: U8 then app({(x) => x + 1}) else app({(x) => x + 2}) end
class C1 is T[U16]
  new create() => None
class C2 is T[U8]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C2.g()(1).string() + " " + C1.g()(1).string())
