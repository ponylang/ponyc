// P.m's default is a literal of S, and S.k calls P.m(), which uses that same
// default. P.m() must compile and print lit: checking the default must not
// need its own result.
actor Main
  new create(env: Env) =>
    env.out.print(P.m())
primitive P
  fun m(x: S val = object val is S fun name(): String => "lit" end): String => x.name()
trait S
  fun name(): String
  fun k(): String => P.m()
