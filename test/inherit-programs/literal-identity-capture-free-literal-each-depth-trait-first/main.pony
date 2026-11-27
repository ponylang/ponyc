// T.m returns a capture-free literal of T. Calling m on class C and again on
// each result expects the same object at every depth. T is declared before C.
trait T
  fun name(): String => "T"
  fun m(): T val => object val is T end
class C is T
actor Main
  new create(env: Env) =>
    let a = C.m()
    let b = a.m()
    let c = b.m()
    env.out.print((a is b).string() + " " + (b is c).string())
