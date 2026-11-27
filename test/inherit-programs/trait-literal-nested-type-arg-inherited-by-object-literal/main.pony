// T.m returns an object literal of U[Array[A] val]. An object literal of T[U8]
// in Main inherits m. Calling m and then n must print u.
trait U[B: Any val]
  fun n(): String => "u"
trait T[A: Any val]
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
actor Main
  new create(env: Env) =>
    let o = object val is T[U8] end
    env.out.print(o.m().n())
