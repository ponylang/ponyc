// T.m's default is a literal of S, and S.k calls C.m(), which uses that same
// default. Calling m on a T val holding C must compile and print lit.
actor Main
  new create(env: Env) =>
    let t: T val = C
    env.out.print(t.m())
class val C is T
trait T
  fun name(): String => "t"
  fun m(x: S val = object val is S fun name(): String => "lit" end): String => x.name()
trait S
  fun name(): String
  fun k(): String => C.m()
