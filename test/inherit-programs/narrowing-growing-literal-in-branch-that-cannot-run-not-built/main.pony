// T.m builds a literal of T[Array[A] val] only where A is Stringable. Q gives
// T[(B | None)]: for U8 expects "n"; for Array[U8] val the branch can't run,
// so its literal isn't checked, and expects "other".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): String => iftype A <: Stringable val then (object val is T[Array[A] val] end).n() else "other" end
class Q[B: Any val]
  new create() => None
  fun make(): T[(B | None)] val => object val is T[(B | None)] end
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].make().m())
    env.out.print(Q[Array[U8] val].make().m())
