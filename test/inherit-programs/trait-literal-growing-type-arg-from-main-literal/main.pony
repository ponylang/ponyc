// T.m returns an object literal of T[Array[A] val], so each call adds a level
// of Array. Starting from an object literal of T[U8] in Main and calling m
// twice must compile and print n.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) =>
    let t: T[U8] val = object val is T[U8] end
    env.out.print(t.m().m().n())
