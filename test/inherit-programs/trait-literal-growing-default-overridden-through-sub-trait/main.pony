// T.m returns an object literal of T[Array[A] val]. G inherits T through T2
// and overrides m to return G[Array[A] val], so calling m twice prints G.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait T2[A: Any val] is T[A]
  fun k(): String => "k"
class val G[A: Any val] is T2[A]
  new val create() => None
  fun m(): T[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"
actor Main
  new create(env: Env) => env.out.print(G[U8].m().m().n())
