// T[A].m() returns a growing T[Array[A] val] literal, and the generic class
// G[B] provides T[B]. G[U8].m().m().m().n() must print "n": each literal's
// class is built with the A of the type it was reached through.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class val G[B: Any val] is T[B]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(G[U8].m().m().m().n())
