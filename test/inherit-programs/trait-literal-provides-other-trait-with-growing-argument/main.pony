// T[A].m() returns a literal of a different trait, U[Array[A] val]. C is
// T[U8], and C.m().n() must print U's default "u": the literal's class must
// be built from the type parameter of the trait it is written in.
trait U[B: Any val]
  fun n(): String => "u"
trait T[A: Any val]
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n())
