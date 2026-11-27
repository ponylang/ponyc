// Foo.f's iftype gives U32(2) in the then branch and 1 in the else branch, so
// the 1 is a U32. Bar is Foo[String]; the else branch runs and prints 1.
trait Foo[A]
  fun f(): String =>
    let x = iftype A <: U8 then U32(2) else 1 end
    x.string()

class Bar is Foo[String]

actor Main
  new create(env: Env) =>
    env.out.print(Bar.f())
