// T[A].m() returns a U[Array[A] val] literal and U[A].k() returns a
// T[Array[A] val] literal, so the two traits refer to each other's types
// without end. No class provides either; the program must print "ok".
trait T[A: Any val]
  fun n(): String => "T"
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
trait U[A: Any val]
  fun n(): String => "U"
  fun k(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) => env.out.print("ok")
