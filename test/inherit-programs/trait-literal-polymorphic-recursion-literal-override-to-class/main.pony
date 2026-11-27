// T[A].m returns an object literal of T[Array[A]]. K's literal overrides m to
// return G, which overrides m and n. K.mk().m().m().n() must print "G".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class val G[A: Any val] is T[A]
  new val create() => None
  fun m(): T[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"
primitive K
  fun mk(): T[U8] val => object val is T[U8] fun m(): T[Array[U8] val] val => G[Array[U8] val] end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().n())
