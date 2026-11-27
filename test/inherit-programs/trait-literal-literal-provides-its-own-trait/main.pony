// T.m returns an object literal that provides T itself, and C inherits m.
// C.m().n() must print "n".
trait T
  fun m(): T val => object val is T end
  fun n(): String => "n"
class C is T
actor Main
  new create(env: Env) => env.out.print(C.m().n())
