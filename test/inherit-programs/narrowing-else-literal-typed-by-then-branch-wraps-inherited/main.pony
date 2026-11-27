// Foo.f's iftype gives U8(2) or 255, so the 255 is a U8. Bar is Foo[String],
// so x is 255 and x + 1 wraps to 0, which is printed.
trait Foo[A]
  fun f(): String =>
    let x = iftype A <: U8 then U8(2) else 255 end
    (x + 1).string()
class Bar is Foo[String]
actor Main
  new create(env: Env) => env.out.print(Bar.f())
