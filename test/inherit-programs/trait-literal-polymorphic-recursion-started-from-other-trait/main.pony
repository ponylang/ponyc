// Maker.mk returns an object literal of T[U8], and T[A].m returns an object
// literal of T[Array[A]]. C inherits mk; calling m() twice on its result and
// then n() must compile and print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait Maker
  fun mk(): T[U8] val => object val is T[U8] end
class C is Maker
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.mk().m().m().n())
