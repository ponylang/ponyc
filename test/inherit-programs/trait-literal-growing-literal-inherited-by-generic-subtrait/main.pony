// T[A].m() returns a growing T[Array[A] val] literal, and the generic
// subtrait T2[B] is T[B]. No class provides either; the program must
// compile and print "ok".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
trait T2[B: Any val] is T[B]
actor Main
  new create(env: Env) => env.out.print("ok")
