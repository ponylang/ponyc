// T.m returns a literal of U, and U.k returns a literal of T; each captures
// its method's argument. Calls through both traits must print u1t2u3.
trait T
  fun name(): String => "T"
  fun m(x: U8): U val => object val is U fun name(): String => "u" + x.string() end
trait U
  fun name(): String => "U"
  fun k(y: U8): T val => object val is T fun name(): String => "t" + y.string() end
class C is T
actor Main
  new create(env: Env) =>
    env.out.print(C.m(1).name() + C.m(1).k(2).name() + C.m(1).k(2).m(3).name())
