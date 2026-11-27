// T.k calls g on a V[A] and V.j calls f on a T[A], each without an
// argument. Each call uses the default of the trait it is made on, so the
// program prints vt.
trait T[A: Any val]
  fun n(): String => "t"
  fun k(v: V[A] val): String => v.g()
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String =>
    x.n()

trait V[A: Any val]
  fun n(): String => "v"
  fun j(t: T[A] val): String => t.f()
  fun g(x: V[Array[A] val] val = object val is V[Array[A] val] end): String =>
    x.n()

class val C is T[U8]
  new val create() => None
class val D is V[U8]
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.k(D) + D.j(C))
