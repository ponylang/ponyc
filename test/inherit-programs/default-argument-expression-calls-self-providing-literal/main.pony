// T.p's default calls name() on a literal that provides T. Calling C.p()
// without an argument must use that default and print T.
trait T
  fun name(): String => "T"
  fun p(x: String = (object val is T end).name()): String => x

class C is T

actor Main
  new create(env: Env) =>
    env.out.print(C.p())
