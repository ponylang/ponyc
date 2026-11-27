// T.p's default argument calls name() on an object literal of T itself, and
// no type provides T. The program must compile and print "hi".
trait T
  fun name(): String => "T"
  fun p(x: String = (object val is T end).name()): String => "c"
actor Main
  new create(env: Env) => env.out.print("hi")
