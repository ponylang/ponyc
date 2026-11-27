// Foo.f's iftype gives U32(2) or 1, so the 1 is a U32. Bar is Foo[String], so
// x is 1 and x - 2 wraps to 4294967295. The wrap shows the literal took its
// type from the then branch even though only the else branch runs.
trait Foo[A]
  fun f(): String =>
    let x = iftype A <: U8 then U32(2) else 1 end
    (x - 2).string()
class Bar is Foo[String]
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f())
