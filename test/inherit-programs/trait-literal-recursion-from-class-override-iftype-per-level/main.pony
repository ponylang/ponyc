// T[A].n uses iftype A <: U8. C provides T[U8] and overrides m with its own
// literal of T[Array[U8]], whose m is T's literal. n() at each level must
// follow that level's type argument: "u8 other other".
trait T[A: Any val]
  fun n(): String => iftype A <: U8 then "u8" else "other" end
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class val C is T[U8]
  new val create() => None
  fun m(): T[Array[U8] val] val => object val is T[Array[U8] val] end
actor Main
  new create(env: Env) =>
    env.out.print(C.n() + " " + C.m().n() + " " + C.m().m().n())
