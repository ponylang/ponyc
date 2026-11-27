// C overrides f() with default 2, and inherits g(), which calls f() with no
// argument. C's copy of g must use C's default; expects 2.
trait T
  fun f(x: U64 = 1): U64 => x
  fun g(): U64 => f()
class val C is T
  new val create() => None
  fun f(x: U64 = 2): U64 => x
actor Main
  new create(env: Env) =>
    env.out.print(C.g().string())
