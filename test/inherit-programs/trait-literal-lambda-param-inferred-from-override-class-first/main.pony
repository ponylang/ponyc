// T.go passes an untyped lambda to take; C, declared before T, overrides take
// with a U64 lambda parameter. Each class must type the lambda from its own
// take: C prints otheru64 and D prints otherother.
primitive Name
  fun apply[X](x: X): String => iftype X <: U64 then "u64" else "other" end

class C is T
  fun take(f: {(U64): String} val): String => f(1)

class D is T

trait T
  fun take(f: {(Any val): String} val): String => f(U64(1))
  fun go(): String => take({(x) => Name[None](None) + Name(x)})

actor Main
  new create(env: Env) =>
    env.out.print(C.go() + " " + D.go())
