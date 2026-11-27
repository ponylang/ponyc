// T[A].m returns an object literal of T[Array[A]]. G overrides m to return
// G[Array[A]] and overrides n, so T's literal is never used for G. Calling
// m().m().n() on G[U8] must compile and print "G".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end

class val G[A: Any val] is T[A]
  new val create() => None
  fun m(): T[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"

actor Main
  new create(env: Env) => env.out.print(G[U8].m().m().n())
