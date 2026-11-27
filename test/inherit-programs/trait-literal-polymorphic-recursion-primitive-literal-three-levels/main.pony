// T[A].m returns an object literal of T[Array[A]]. Calling m() three times
// on the T[U8] object literal that primitive K returns, then n(), must
// compile and print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().m().n())
