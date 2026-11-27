// T.p()'s default argument is an object literal of T's subtrait U. No class
// provides T or U; the program must compile and print "hi": the default
// must be checked once without the compiler failing on it.
trait T
  fun name(): String => "T"
  fun p(x: T val = object val is U end): String => x.name()
trait U is T
actor Main
  new create(env: Env) => env.out.print("hi")
