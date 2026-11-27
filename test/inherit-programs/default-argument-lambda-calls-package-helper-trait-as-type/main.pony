// A trait in lib has a default lambda that calls lib's Helper. Main uses the
// trait only as a type, so the default is never called. The program must
// compile; expects built.
use l = "lib"
actor Main
  new create(env: Env) =>
    let t: (l.T | None) = None
    env.out.print("built")
