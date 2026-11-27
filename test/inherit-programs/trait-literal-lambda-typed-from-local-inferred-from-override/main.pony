// T.go assigns an untyped lambda to f, a local whose type comes from take().
// C's take returns a lambda over Any val, T's one over U64. Each class must
// type the lambda from its own take: C prints other and D prints u64.
primitive Name
  fun apply[X](x: X): String => iftype X <: U64 then "u64" else "other" end

trait T
  fun take(): {(U64): String} val => {(x) => "t"}
  fun go(): String =>
    var f = take()
    f = {(x) => Name(x)}
    f(U64(1))

class C is T
  fun take(): {(Any val): String} val => {(x) => "c"}

class D is T

actor Main
  new create(env: Env) =>
    env.out.print(C.go() + " " + D.go())
