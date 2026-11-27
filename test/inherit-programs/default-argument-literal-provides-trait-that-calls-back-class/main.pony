// T.m's default is a literal of S, and S.k calls C.m(), which uses that same
// default. C.m() must compile and print lit: checking the default must not
// need its own result.
actor Main
  new create(env: Env) =>
    env.out.print(C.m())
class val C is T
trait T
  fun name(): String => "t"
  fun m(x: S val = object val is S fun name(): String => "lit" end): String => x.name()
trait S
  fun name(): String
  fun k(): String => C.m()
