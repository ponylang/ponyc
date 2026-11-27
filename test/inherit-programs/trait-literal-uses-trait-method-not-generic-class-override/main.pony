// G[A] overrides T.m() and n(). G[U8].p() returns a T literal, and that
// literal's m() is T's, not G's, so G[U8].p().m().n() must print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun p(): T[Array[A] val] val => object val is T[Array[A] val] end
class val G[A: Any val] is T[A]
  new val create() => None
  fun m(): T[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"
actor Main
  new create(env: Env) => env.out.print(G[U8].p().m().n())
