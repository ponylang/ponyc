// Inside f()'s iftype A <: Mut ref branch, a.mutate("s") resolves to Bar's
// mutate(), which takes a U8. Expects "argument not assignable to parameter"
// twice: once for T and once for C's copy, with a note naming method 'f'
// inherited by C from T.
class Bar
  var n: U8 = 0
  new create() => None
  fun ref mutate(x: U8): U8 => n = n + x; n
trait val Mut
  fun ref mutate(x: String): U8
trait T[A: Bar #read]
  fun f(a: A): String =>
    iftype A <: Mut ref then a.mutate("s").string() else "other" end
class C is T[Bar val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(recover val Bar end))
