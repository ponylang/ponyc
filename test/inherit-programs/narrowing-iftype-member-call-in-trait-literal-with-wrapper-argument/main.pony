// T[A].s() calls a.string() only when A is Stringable. C is T[U8] and prints
// "3"; C.m() and C.m().m() are T[W[...]] literals, and W is Stringable, so
// each s() prints "w". The call must be found through the narrowing in each
// literal's copy.
class val W[X: Any val] is Stringable
  let x: X
  new val create(x': X) => x = x'
  fun string(): String iso^ => "w".clone()
trait T[A: Any val]
  fun s(a: A): String => iftype A <: Stringable val then a.string() else "?" end
  fun m(): T[W[A]] val => object val is T[W[A]] end
  fun w(a: A): W[A] => W[A](a)
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.s(3) + " " + C.m().s(C.w(3)) + " " + C.m().m().s(W[W[U8]](W[U8](3))))
