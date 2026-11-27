// An object literal overrides f with default 2 and inherits g, which calls
// f(). Expects "2": the inherited g uses the literal's f and its default, as
// it would for a named class.
trait T
  fun f(x: U64 = 1): U64 => x
  fun g(): U64 => f()
actor Main
  new create(env: Env) =>
    let o = object val is T fun f(x: U64 = 2): U64 => x end
    env.out.print(o.g().string())
