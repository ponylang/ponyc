// T[A].s() calls a.string() only when A is Stringable. C is T[U8] and prints
// "3"; C.m() and C.m().m() are T[(U8 | None)] literals, which are Stringable,
// so s(None) prints "None". The call must be found through the narrowing in
// each literal's copy.
trait T[A: Any val]
  fun s(a: A): String => iftype A <: Stringable val then a.string() else "?" end
  fun m(): T[(A | None)] val => object val is T[(A | None)] end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.s(3) + " " + C.m().s(None) + " " + C.m().m().s(None))
