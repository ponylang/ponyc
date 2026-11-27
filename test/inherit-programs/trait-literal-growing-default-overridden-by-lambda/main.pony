// T.m returns a lambda that builds an object literal of T[Array[A] val]. G
// overrides m with a lambda that builds G[Array[A] val] instead, so the
// program prints G.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): {(): T[Array[A] val] val} val => {(): T[Array[A] val] val => object val is T[Array[A] val] end}
class val G[A: Any val] is T[A]
  new val create() => None
  fun m(): {(): T[Array[A] val] val} val => {(): T[Array[A] val] val => G[Array[A] val]}
  fun n(): String => "G"
actor Main
  new create(env: Env) => env.out.print(G[U8].m()().n())
