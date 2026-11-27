// T[A].m() returns a growing T[Array[A] val] literal, and the generic class
// G[B] provides T[B]. m() is never called; G[U8].n() must print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class val G[B: Any val] is T[B]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(G[U8].n())
