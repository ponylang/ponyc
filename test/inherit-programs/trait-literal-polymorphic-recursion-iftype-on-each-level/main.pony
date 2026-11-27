// T[A].n uses iftype A <: U8 and T[A].m returns an object literal of
// T[Array[A]]. Starting from an object literal of T[U8], n() must print
// "u8" at the first level and "other" at the next two.
trait T[A: Any val]
  fun n(): String => iftype A <: U8 then "u8" else "other" end
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) =>
    let o = object val is T[U8] end
    env.out.print(o.n() + " " + o.m().n() + " " + o.m().m().n())
