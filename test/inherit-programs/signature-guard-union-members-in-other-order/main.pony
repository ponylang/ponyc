// U's guarded g tests A <: (U32 val | U8 val) and D's override writes the
// same union in the other order, so the two guards are the same and D's g
// overrides U's. The test expects D's g for both calls, printing 2 2 (#6728).
trait U
  fun g[A: Any val](a: A): U8 => 0
  fun g[A: Any val](a: A): U8 iftype A <: (U32 val | U8 val) => 1

class D is U
  fun g[A: Any val](a: A): U8 iftype A <: (U8 val | U32 val) => 2

actor Main
  new create(env: Env) =>
    let u: U = D
    env.out.print(D.g[U32](1).string() + " " + u.g[U32](1).string())
