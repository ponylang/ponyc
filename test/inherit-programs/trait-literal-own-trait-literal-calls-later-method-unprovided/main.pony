// T.p makes an object literal of T and calls name, declared after p. No type
// provides T; the program must compile and print "hi".
trait T
  fun p(s: String): String => (object val is T end).name() + s
  fun name(): String => "T"
actor Main
  new create(env: Env) => env.out.print("hi")
