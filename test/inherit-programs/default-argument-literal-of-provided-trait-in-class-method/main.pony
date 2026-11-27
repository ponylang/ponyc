// C provides T, and C.p's default argument is an object literal of T. C.p()
// uses the default and must print "T".
trait T
  fun name(): String => "T"
class C is T
  fun p(x: T val = object val is T end): String => x.name()
actor Main
  new create(env: Env) => env.out.print(C.p())
