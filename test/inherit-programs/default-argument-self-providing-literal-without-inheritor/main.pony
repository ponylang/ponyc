// T.p's default is a literal that provides T. Nothing provides T and
// nothing calls p; the program must still compile and print hi.
trait T
  fun name(): String => "T"
  fun p(x: T val = object val is T end): String => x.name()

actor Main
  new create(env: Env) =>
    env.out.print("hi")
