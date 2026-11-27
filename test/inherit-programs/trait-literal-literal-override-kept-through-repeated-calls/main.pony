// T.mk returns an object literal of T that overrides name. Calling mk() on C
// and then twice more on the literal must print "lit".
trait T
  fun name(): String => "T"
  fun mk(): T val => object val is T fun name(): String => "lit" end
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().mk().mk().name())
