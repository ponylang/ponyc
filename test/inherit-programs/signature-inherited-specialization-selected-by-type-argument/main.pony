// Class C inherits a general f() and one guarded by `iftype X <: U32 val`.
// The test expects "1 0": U32 selects the guarded body, String the general.
// Issue #6725.
trait T
  fun f[X: Stringable](x: X): U8 => 0
  fun f[X: Stringable](x: X): U8 iftype X <: U32 val => 1

class C is T

actor Main
  new create(env: Env) =>
    env.out.print(C.f[U32](1).string() + " " + C.f[String]("a").string())
