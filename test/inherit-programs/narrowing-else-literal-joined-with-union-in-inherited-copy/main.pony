// Foo.f's iftype gives mk(), a (U8 | String), or the literal 1. C is
// Foo[String], so the else branch runs; 1 must take a numeric type, and the
// program prints 1.
trait Foo[A: Any val]
  fun mk(): (U8 | String)
  fun f(): String =>
    let x = iftype A <: U8 then mk() else 1 end
    x.string()
class C is Foo[String]
  new create() => None
  fun mk(): (U8 | String) => "s"
actor Main
  new create(env: Env) => env.out.print(C.f())
