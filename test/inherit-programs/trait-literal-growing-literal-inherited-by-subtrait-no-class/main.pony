// T[A].m() returns a growing T[Array[A] val] literal, and the subtrait T2 is
// T[U8]. No class provides either; the program must compile and print "ok".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait T2 is T[U8]
actor Main
  new create(env: Env) => env.out.print("ok")
