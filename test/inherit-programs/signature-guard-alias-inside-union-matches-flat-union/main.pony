// U's specialization of g is guarded by A <: (U8 | U16 | U32) and D's by
// A <: (Small | U32), where Small is (U8 | U16): the same type, written with
// an alias inside the union. D's g overrides U's, so the test expects 2 for
// both calls (#6728).
type Small is (U8 | U16)

trait U
  fun g[A: Any val](a: A): U8 => 0
  fun g[A: Any val](a: A): U8 iftype A <: (U8 | U16 | U32) => 1

class D is U
  fun g[A: Any val](a: A): U8 iftype A <: (Small | U32) => 2

actor Main
  new create(env: Env) =>
    let u: U = D
    env.out.print(D.g[U32](1).string() + " " + u.g[U32](1).string())
