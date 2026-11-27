// T.p's default argument is an object literal of U, declared before T. C
// inherits p and C.p() must print "u".
trait U
  fun u(): String => "u"
trait T
  fun p(x: U val = object val is U end): String => x.u()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.p())
