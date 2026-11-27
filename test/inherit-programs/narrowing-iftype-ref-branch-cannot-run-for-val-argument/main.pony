// C fills A with Bar val, so f()'s iftype A <: Bar ref branch, which calls
// the ref method mutate(), can't run in C's copy. Expects "other": calling a
// ref method on a val in that branch is not an error.
class Bar
  var n: U8 = 0
  new create() => None
  fun ref mutate(): U8 => n = n + 1; n
trait T[A: Bar #read]
  fun f(a: A): String =>
    iftype A <: Bar ref then a.mutate().string() else "other" end
class C is T[Bar val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(recover val Bar end))
