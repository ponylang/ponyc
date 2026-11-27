// T has a default f and an f specialized for R <: U32. An object literal from
// a generic method provides T[Q]. Expects "R2" for Q = U32 and "d" for Q = U8:
// each literal gets the body its type argument selects (#6735).
trait T[R: Any val]
  fun f[X: Any val](x: X, r: R): String => "d"
  fun f[X: Any val](x: X, r: R): String iftype R <: U32 val => "R" + r.string()

primitive Make
  fun apply[Q: Any val](): T[Q] val => object val is T[Q] end

actor Main
  new create(env: Env) =>
    env.out.print(Make[U32]().f[U8](1, 2) + Make[U8]().f[U32](3, 4))
