// T.p's default is a literal that provides T. Calling C.p() from Main
// without an argument must use that default and print T.
trait T
  fun name(): String => "T"
  fun p(x: T val = object val is T end): String => x.name()

class C is T

actor Main
  new create(env: Env) =>
    env.out.print(C.p())
