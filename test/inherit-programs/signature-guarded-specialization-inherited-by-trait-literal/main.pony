// Mk.apply returns a literal of T[Q], and T.f has a specialization guarded
// on R <: U32. With Q = U32 the guarded body runs and with Q = U8 the
// default body runs, so the program prints R2d (#6735).
trait T[R: Any val]
  fun f[X: Any val](x: X, r: R): String => "d"
  fun f[X: Any val](x: X, r: R): String iftype R <: U32 val => "R" + r.string()
trait Mk
  fun apply[Q: Any val](): T[Q] val => object val is T[Q] end
primitive Make is Mk
actor Main
  new create(env: Env) =>
    env.out.print(Make[U32]().f[U8](1, 2) + Make[U8]().f[U32](3, 4))
