// An object literal's mk returns a captured B2, so in its copy of f, string()
// is found only through the iftype on A, which may not hold for the literal.
// Expects that error with the hint to bind a local first.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
class Q[B2: Any val]
  let _b: B2
  new create(b: B2) => _b = b
  fun make(): Foo[(B2 | None)] =>
    let b = _b
    object is Foo[(B2 | None)] fun mk(): B2 => b end
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).make().f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).make().f())
