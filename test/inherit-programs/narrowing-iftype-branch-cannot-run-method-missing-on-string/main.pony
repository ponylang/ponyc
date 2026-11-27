// C fills A with String, so f()'s iftype A <: Bar ref branch, which calls
// mutate(), can't run in C's copy. Expects "other": String having no mutate()
// is not an error in that branch.
class Bar
  new create() => None
  fun ref mutate(): U8 => 1
trait T[A: Any #read]
  fun f(a: A): String =>
    iftype A <: Bar ref then a.mutate().string() else "other" end
class C is T[String val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f("x"))
