// T.m returns an object literal of T[Array[A] val], so each call adds a level
// of Array. Starting from P's literal and calling m three times must compile
// and print n.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
primitive P
  fun f(): T[Array[U8] val] val => object val is T[Array[U8] val] end
actor Main
  new create(env: Env) => env.out.print(P.f().m().m().m().n())
