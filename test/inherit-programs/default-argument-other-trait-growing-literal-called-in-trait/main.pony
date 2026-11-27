// V.f's default is a literal of T[Array[A] val], and T.g calls f on a
// V[A] without an argument, so the type arguments grow. C.g(D) must print n.
trait V[A: Any val]
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String =>
    x.n()
trait T[A: Any val]
  fun n(): String => "n"
  fun g(v: V[A] val): String => v.f()
class val C is T[U8]
  new val create() => None
class val D is V[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.g(D))
