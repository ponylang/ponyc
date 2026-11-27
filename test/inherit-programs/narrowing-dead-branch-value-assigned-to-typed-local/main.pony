// Bar inherits f with A = String, so the iftype's then branch, which assigns
// x to a U8 local, can't run. That assignment must not be an error, and the
// else value 7 plus one prints 8.
trait Foo[A: Any val]
  fun f(x: A): U8 =>
    let y: U8 = iftype A <: U8 then x else U8(7) end
    y + 1
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f("s").string())
