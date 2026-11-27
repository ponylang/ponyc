// V.f's default is a literal of T, and T.g calls f on a V without an
// argument. C.g(D) uses V's default and prints n.
trait V
  fun f(x: T val = object val is T end): String => x.n()
trait T
  fun n(): String => "n"
  fun g(v: V val): String => v.f()
class val C is T
  new val create() => None
class val D is V
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.g(D))
