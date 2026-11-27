// Bar[A].f joins a, narrowed to U8 in the then branch, with U8(5), so x is a
// U8. Bar[String] passes x to Show, which prints exact.
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: U8 then "exact" else "wide" end
class Bar[A: Any val]
  new create() => None
  fun f(a: A): String =>
    let x = iftype A <: U8 then a else U8(5) end
    Show(x)
actor Main
  new create(env: Env) => env.out.print(Bar[String].f("s"))
