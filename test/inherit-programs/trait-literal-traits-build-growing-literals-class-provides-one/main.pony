// T[A].m() returns a U[Array[A] val] literal and U[A].k() returns a
// T[Array[A] val] literal. C provides U[U8], so its k() must be copied from
// U while both traits are checked; the program must compile and print "ok".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): U[Array[A] val] val => object val is U[Array[A] val] end
trait U[A: Any val]
  fun k(): T[Array[A] val] val => object val is T[Array[A] val] end
class C is U[U8]
actor Main
  new create(env: Env) => env.out.print("ok")
