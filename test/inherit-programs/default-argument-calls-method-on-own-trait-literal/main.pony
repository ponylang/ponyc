// T.r()'s default argument builds a T literal and calls its p(), which uses
// its own default. No class provides T; the program must compile and print
// "hi": the default must be checked once without the compiler failing on it.
trait T
  fun name(): String => "T"
  fun p(x: String = "d"): String => x
  fun r(y: String = (object val is T end).p()): String => y
actor Main
  new create(env: Env) => env.out.print("hi")
