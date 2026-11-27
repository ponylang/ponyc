// T.mk returns an object literal of U, and U.hello creates an object literal
// of V. C inherits mk; calling hello() on its result must print "v".
trait V
  fun v(): String => "v"
trait U
  fun hello(): String => (object val is V end).v()
trait T
  fun mk(): U val => object val is U end
class C is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.mk().hello())
