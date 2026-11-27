// Generic trait T[A] returns a capture-free literal of T[A]. C1 and C2 both
// provide T[U8]. Expects their results to be the same object.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[A] val => object val is T[A] end
class C1 is T[U8]
  new create() => None
class C2 is T[U8]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C1.m() is C2.m()).string() + " " + (C1.m() is C1.m()).string())
