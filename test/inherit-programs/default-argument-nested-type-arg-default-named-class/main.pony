// V.f's default is W[A], a named generic class that is T[Array[A] val]. T.g
// calls f() with the default. Must compile without growing types; expects n.
trait V[A: Any val]
  fun f(x: T[Array[A] val] val = W[A]): String => x.n()
class val W[A: Any val] is T[Array[A] val]
  new val create() => None
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
