// T's val method m returns a capture-free literal of T. Expects the results
// at each depth, from class C and from class D, to be the same object.
trait T
  fun val m(): T val => object val is T end
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let a = C.m(); let b = a.m(); let d = b.m()
    let e = D.m(); let f = e.m()
    env.out.print((a is b).string() + " " + (b is d).string() + " " + (a is e).string() + " " + (b is f).string())
