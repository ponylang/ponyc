// The interface T's p() has a default argument that is an object literal of
// T. No class provides T; the program must compile and print "hi": the
// default must be checked once without the compiler failing on it.
interface T
  fun name(): String => "T"
  fun p(x: T val = object val is T end): String => x.name()
actor Main
  new create(env: Env) => env.out.print("hi")
