// T.m's default is a literal that provides T. Calling m without an
// argument on a T val holding C must use T's default and print lit.
actor Main
  new create(env: Env) =>
    let t: T val = C
    env.out.print(t.m())
class val C is T
trait T
  fun name(): String => "t"
  fun m(x: T val = object val is T fun name(): String => "lit" end): String => x.name()
