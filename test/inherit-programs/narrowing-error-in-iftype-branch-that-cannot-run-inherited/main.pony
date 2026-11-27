// Foo.f is partial only because the iftype's then branch raises an error. Bar
// is Foo[String], so the else branch runs and the program prints 0.
trait Foo[A: Any val]
  fun f(): U8 ? =>
    iftype A <: U8 then error else 0 end
class Bar is Foo[String]
actor Main
  new create(env: Env) => try env.out.print(Bar.f()?.string()) end
