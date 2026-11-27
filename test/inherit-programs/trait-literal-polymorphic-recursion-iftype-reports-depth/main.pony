// T[A].m returns an object literal of T[Array[A]]. lvl() uses iftype on A to
// report how deeply the type argument is nested. Starting from T[U8] and
// calling m() up to three times must print "0123+": each level's literal
// keeps its own type argument.
trait T[A: Any val]
  fun lvl(): String =>
    iftype A <: U8 then "0"
    elseif A <: Array[U8] val then "1"
    elseif A <: Array[Array[U8] val] val then "2"
    else "3+" end
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) =>
    let o = object val is T[U8] end
    env.out.print(o.lvl() + o.m().lvl() + o.m().m().lvl() + o.m().m().m().lvl())
