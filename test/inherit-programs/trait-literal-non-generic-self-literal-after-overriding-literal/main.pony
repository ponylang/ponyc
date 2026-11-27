// T.m returns an object literal of T, with no type argument to grow. K's
// literal overrides n, but the literals from m do not, so
// K.mk().m().m().n() must print "n".
trait T
  fun n(): String => "n"
  fun m(): T val => object val is T end
primitive K
  fun mk(): T val => object val is T fun n(): String => "k" end
actor Main
  new create(env: Env) => env.out.print(K.mk().m().m().n())
