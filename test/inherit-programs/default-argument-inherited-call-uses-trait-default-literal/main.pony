// A trait literal overrides f() with default 2 and inherits g(), which calls
// f() with no argument. The literal's g keeps T's default; expects 1.
trait T
  fun f(x: U64 = 1): U64 => x
  fun g(): U64 => f()
trait M
  fun mk(): T val => object val is T fun f(x: U64 = 2): U64 => x end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().g().string())
