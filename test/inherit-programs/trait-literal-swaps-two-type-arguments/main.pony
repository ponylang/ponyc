// T[A1, A2].swp() returns a T[A2, A1] literal. C.swp().swp().name() must
// print "t": a literal whose type arguments are a reordering of the trait's
// must not make the compiler expand T forever.
trait T[A1, A2]
  fun swp(): T[A2, A1] val => object is T[A2, A1] end
  fun name(): String => "t"
class C is T[U8, U16]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.swp().swp().name())
