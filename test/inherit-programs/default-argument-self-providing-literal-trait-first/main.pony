// T.m's default is a literal that provides T, with T declared first. C.m()
// uses the default and C.m(C) uses C, printing litt.
trait T
  fun name(): String => "t"
  fun m(x: T val = object val is T fun name(): String => "lit" end): String => x.name()
class val C is T
actor Main
  new create(env: Env) => env.out.print(C.m() + C.m(C))
