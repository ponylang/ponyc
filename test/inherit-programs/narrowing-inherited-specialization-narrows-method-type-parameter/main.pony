// Primitive P inherits a general g() and one guarded by
// `iftype A <: Stringable val` that calls x.string(). The test expects 9: the
// guard narrows A in the inherited body, and the guarded body is used for U32.
// Issue #6725.
trait Tr
  fun g[A: Any val](x: A): String => "no"
  fun g[A: Any val](x: A): String iftype A <: Stringable val => x.string()

primitive P is Tr

actor Main
  new create(env: Env) =>
    env.out.print(P.g[U32](9))
