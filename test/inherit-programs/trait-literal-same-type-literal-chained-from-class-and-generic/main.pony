// T[A].mk() returns a T[A] literal. C is T[U8] and the generic G[B] is T[B];
// C.mk().mk().name() and G[String].mk().mk().name() must each print "T".
trait T[A: Any val]
  fun name(): String => "T"
  fun mk(): T[A] val => object val is T[A] end
class C is T[U8]
  new create() => None
class G[B: Any val] is T[B]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().mk().name())
    env.out.print(G[String].mk().mk().name())
