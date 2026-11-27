// T.f's default is a literal of T[Array[A] val]. C.k(), which calls f(),
// and C.f() both use C's copy of that default and print nn.
trait T[A: Any val]
  fun n(): String => "n"
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String =>
    x.n()
  fun k(): String => f()

class val C is T[U8]
  new val create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.k() + C.f())
