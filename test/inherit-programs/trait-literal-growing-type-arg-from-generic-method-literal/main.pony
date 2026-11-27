// T.m returns an object literal of T[Array[A] val], so each call adds a level
// of Array. Starting from a literal built in the generic method P.f[U8] and
// calling m twice must compile and print n.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
primitive P
  fun f[B: Any val](): T[B] val => object val is T[B] end
actor Main
  new create(env: Env) => env.out.print(P.f[U8]().m().m().n())
