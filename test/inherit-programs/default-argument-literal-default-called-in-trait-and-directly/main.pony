// T.p's default calls a literal's n(). T.m calls p on a literal that
// provides T, and Main calls C.p(); both use the default and print o.
trait T
  fun m(): String => (object val is T end).p()
  fun p(x: String = (object val fun n(): String => "o" end).n()): String => x
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.m() + C.p())
