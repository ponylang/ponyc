// T[A].m() returns a growing T[Array[A] val] literal, and C, which declares
// its own constructor, is T[U8]. C.m().m().m().n() must print "n": each
// literal's class is built with the A of the type it was reached through.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class C is T[U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().n())
