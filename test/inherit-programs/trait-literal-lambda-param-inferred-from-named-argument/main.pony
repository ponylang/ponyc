// T.go passes an untyped lambda as take's named argument f. C overrides take
// with a U64 lambda parameter; D inherits T's Any val one. Each class must type
// the lambda from its own take: C prints otheru64 and D prints otherother.
primitive Name
  fun apply[X](x: X): String => iftype X <: U64 then "u64" else "other" end

trait T
  fun take(n: U8 = 0, f: {(Any val): String} val = {(x) => "d"}): String => f(U64(1))
  fun go(): String => take(where f = {(x) => Name[None](None) + Name(x)})

class C is T
  fun take(n: U8 = 0, f: {(U64): String} val = {(x) => "d"}): String => f(1)

class D is T

actor Main
  new create(env: Env) =>
    env.out.print(C.go() + " " + D.go())
