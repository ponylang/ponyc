// f() binds v to an iftype whose then branch returns x, and C fills A with P,
// which is not Stringable. Outside the iftype, v is (P | String), so assigning
// it to a Stringable fails. Expects "right side must be a subtype of left
// side" with a note naming method 'f' inherited by C from Foo.
primitive P
trait Foo[A: Any val]
  fun f(x: A): String =>
    let v = iftype A <: Stringable val then x else "s" end
    let w: Stringable val = v
    w.string()
class C is Foo[P]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(P))
