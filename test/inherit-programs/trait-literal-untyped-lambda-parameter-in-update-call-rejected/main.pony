// A trait default passes `{(x) => ...}` as the value of `this(0) = ...`. A
// lambda parameter needs a type or a type from its context, so the test
// expects that error even though the update method gives the lambda a type.
primitive Name
  fun apply[X](x: X): String => iftype X <: U64 then "u64" else "other" end

trait T
  fun update(i: U8, f: {(Any val): String} val): String => f(U64(1))
  fun go(): String => this(0) = {(x) => Name[None](None) + Name(x)}

class C is T
  fun update(i: U8, f: {(U64): String} val): String => f(1)

class D is T

actor Main
  new create(env: Env) =>
    env.out.print(C.go() + " " + D.go())
