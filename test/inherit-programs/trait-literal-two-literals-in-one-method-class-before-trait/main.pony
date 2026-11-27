// T.m returns a tuple of two object literals of T, and C inherits m. Both
// literals must inherit T's name, so the program prints "TT". C is declared
// before T.
class C is T
actor Main
  new create(env: Env) =>
    (let a, let b) = C.m()
    env.out.print(a.name() + b.name())
trait T
  fun name(): String => "T"
  fun m(): (T val, T val) => (object val is T end, object val is T end)
