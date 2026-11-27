// Q[B] is T[(B | None)], so whether T.m()'s iftype branch runs depends on B.
// Q[U8] takes the branch and prints "n"; Q[Array[U8] val] prints "other".
// The growing literal in the branch must be typed per instantiation of Q.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): String => iftype A <: Stringable val then (object val is T[Array[A] val] end).n() else "other" end
class val Q[B: Any val] is T[(B | None)]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].m())
    env.out.print(Q[Array[U8] val].m())
