// Bar fills A with String, so the iftype A <: U8 branch can't run, and the
// else branch is the literal 7. In Bar's copy the if is (String | U8), so
// assigning it to the U8 y fails. Expects "right side must be a subtype of left
// side" with a note naming method 'f' inherited by Bar from Foo.
trait Foo[A: Any val]
  fun f(x: A): U8 =>
    let y: U8 = iftype A <: U8 then x else 7 end
    y + 1
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f("s").string())
