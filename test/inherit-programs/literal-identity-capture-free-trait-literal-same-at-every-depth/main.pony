// T.m returns an object literal of T with no captures. Expects every result
// of m, from C1, from C2 and from the literal itself, to be the same object.
trait T
  fun n(): String => "n"
  fun m(): T val => object val is T end
class C1 is T
  new create() => None
class C2 is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C1.m() is C2.m()).string() + " " + (C1.m() is C1.m()).string() + " " + (C1.m().m() is C2.m().m()).string() + " " + (C1.m() is C1.m().m()).string())
