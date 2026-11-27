// Bar is Foo[String] and Baz is Foo[U8]. The early return runs only in Baz's
// copy of f(); expects late, then early.
trait Foo[A: Any val]
  fun f(): String =>
    iftype A <: U8 then return "early" else None end
    "late"
class Bar is Foo[String]
class Baz is Foo[U8]
actor Main
  new create(env: Env) => env.out.print(Bar.f()); env.out.print(Baz.f())
