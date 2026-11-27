// Foo.f joins a, of type A, from the then branch with U8(5) from the else
// branch. Bar is Foo[String], so x is a (String | U8) and Show prints wide.
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: U8 then "exact" else "wide" end
trait Foo[A: Any val]
  fun f(a: A): String =>
    let x = iftype A <: U8 then a else U8(5) end
    Show(x)
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f("s"))
