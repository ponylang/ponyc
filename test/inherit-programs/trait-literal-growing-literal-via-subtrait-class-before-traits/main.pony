// T[A].m() returns a growing T[Array[A] val] literal, T2 is T[U8], and C is
// T2. C is declared before the traits. C.m().m().n() must print "n", the
// same as when C comes after them.
class C is T2
  new create() => None
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait T2 is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.m().m().n())
