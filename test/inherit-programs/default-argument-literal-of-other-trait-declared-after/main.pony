// T.p's default argument is an object literal of U, declared after T, and
// U.u has a default argument of its own. C inherits p and C.p() must print
// "u".
trait T
  fun p(x: U val = object val is U end): String => x.u()
trait U
  fun u(y: String = "u"): String => y
class C is T
actor Main
  new create(env: Env) => env.out.print(C.p())
