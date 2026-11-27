// T.m() returns a U literal and U.k() returns a T literal. C is T, and
// C.m().k().m().name() must print "U": the two traits' literals must be
// built in turn without the compiler recursing forever.
trait T
  fun name(): String => "T"
  fun m(): U val => object val is U end
trait U
  fun name(): String => "U"
  fun k(): T val => object val is T end
class C is T
actor Main
  new create(env: Env) => env.out.print(C.m().k().m().name())
