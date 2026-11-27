// T.p's default argument is an object literal of T itself and p never uses
// it. No type provides T; the program must compile and print "hi".
trait T
  fun name(): String => "T"
  fun p(x: T val = object val is T end): String => "const"
actor Main
  new create(env: Env) => env.out.print("hi")
