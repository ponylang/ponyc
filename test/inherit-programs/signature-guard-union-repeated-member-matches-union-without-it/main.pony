// Trait U has a general g() and one guarded by
// `iftype A <: (U32 val | U64 val)`. D overrides that guarded body with the
// guard's union written with U32 val twice, which is the same guard. The test
// expects "2 2": D's body replaces U's, called on D or through U.
trait U
  fun g[A: Any val](a: A): U8 => 0
  fun g[A: Any val](a: A): U8 iftype A <: (U32 val | U64 val) => 1

class D is U
  fun g[A: Any val](a: A): U8 iftype A <: (U32 val | U32 val | U64 val) => 2

actor Main
  new create(env: Env) =>
    let u: U = D
    env.out.print(D.g[U32](1).string() + " " + u.g[U32](1).string())
