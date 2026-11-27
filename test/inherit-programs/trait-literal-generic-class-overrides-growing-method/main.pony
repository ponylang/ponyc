// G[A] overrides T.m(), which returns a growing T[Array[A] val] literal, with
// a method returning G[Array[A] val]. G[U8].m().m().n() must print "G", and
// H, which keeps T's m(), must print "n", so the program prints "Gn".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class val G[A: Any val] is T[A]
  new val create() => None
  fun m(): T[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"
class val H is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(G[U8].m().m().n() + H.n())
