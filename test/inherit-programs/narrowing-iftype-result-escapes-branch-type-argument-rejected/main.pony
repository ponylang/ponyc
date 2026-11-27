// f() binds v to an iftype whose then branch returns x, and C fills A with P,
// which is not Stringable. Outside the iftype, v is (P | String), so passing
// it to Show[T: Stringable] fails. Expects "type argument is outside its
// constraint" with a note naming method 'f' inherited by C from Foo.
primitive P
primitive Show
  fun apply[T: Stringable val](t: T): String => t.string()
trait Foo[A: Any val]
  fun f(x: A): String =>
    let v = iftype A <: Stringable val then x else "s" end
    Show(v)
class C is Foo[P]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(P))
