// T[A].f()'s default argument is a T[Array[A] val] literal. C is T[U8] and
// overrides n(). Calling f() on C through a T[U8] must use the default
// literal, whose n() is T's, and print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String => x.n()
class C is T[U8]
  fun n(): String => "C"
actor Main
  new create(env: Env) =>
    let t: T[U8] = C
    env.out.print(t.f())
