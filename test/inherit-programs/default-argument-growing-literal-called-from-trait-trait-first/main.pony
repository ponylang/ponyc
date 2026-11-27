// T.f's default is a literal of T[Array[A] val], and T.k calls f() without
// an argument. C.k() uses C's copy of that default and prints n.
trait T[A: Any val]
  fun n(): String => "n"
  fun k(): String => f()
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String =>
    x.n()

class val C is T[U8]
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.k())
