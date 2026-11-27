// T.p's default is a literal that provides T, and T.m calls C.p() without
// an argument. The default literal's name is T's, so the program prints T.
trait T
  fun name(): String => "T"
  fun p(x: T val = object val is T end): String => x.name()
  fun m(): String => C.p()

class C is T

actor Main
  new create(env: Env) =>
    env.out.print(C.m())
