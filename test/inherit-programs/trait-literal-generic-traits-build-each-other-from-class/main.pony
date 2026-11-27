// T[A].m() returns a U[A] literal and U[A].k() returns a T[A] literal. C is
// T[U8], and C.m().k().m().n() must print "U": the two traits' literals must
// be built in turn without the compiler recursing forever.
trait T[A: Any val]
  fun n(): String => "T"
  fun m(): U[A] val => object val is U[A] end
trait U[A: Any val]
  fun n(): String => "U"
  fun k(): T[A] val => object val is T[A] end
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.m().k().m().n())
