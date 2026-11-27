// Trait U has a general g() and one guarded by `iftype A <: U32 val`. D
// overrides that guarded body with the guard written through an alias of U32.
// The test expects "2 2": D's body replaces U's, called on D or through U.
// Issue #6728.
type Num is U32

trait U
  fun g[A: Any val](a: A): U8 => 0
  fun g[A: Any val](a: A): U8 iftype A <: U32 val => 1

class D is U
  fun g[A: Any val](a: A): U8 iftype A <: Num val => 2

actor Main
  new create(env: Env) =>
    let u: U = D
    env.out.print(D.g[U32](1).string() + " " + u.g[U32](1).string())
