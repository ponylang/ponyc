// T[A].f()'s default argument is a T[Array[A] val] literal. No class provides
// T; the program must compile and print "ok": the default argument must be
// checked once without the compiler failing on it.
trait T[A: Any val]
  fun n(): String => "n"
  fun f(x: T[Array[A] val] val = object val is T[Array[A] val] end): String => x.n()
actor Main
  new create(env: Env) => env.out.print("ok")
