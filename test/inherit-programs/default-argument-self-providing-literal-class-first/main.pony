// T.m's default is a literal that provides T, with Main and C declared
// before T. C.m() uses the default and C.m(C) uses C, printing litt.
actor Main
  new create(env: Env) => env.out.print(C.m() + C.m(C))
class val C is T
trait T
  fun name(): String => "t"
  fun m(x: T val = object val is T fun name(): String => "lit" end): String => x.name()
