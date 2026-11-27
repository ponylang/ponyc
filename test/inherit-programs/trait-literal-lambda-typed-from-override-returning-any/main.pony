// T.go passes {(x) => x + 1} to run. C's run takes {(U8): Any val}, D's takes
// {(U8): U8}. In both, 255 + 1 is a U8 and wraps to 0, so C prints 1000 and
// D prints 0.
trait T
  fun run(f: {(U8): U8} val): U64
  fun go(): U64 => run({(x) => x + 1})
class C is T
  new create() => None
  fun run(f: {(U8): Any val} val): U64 => match f(255) | let u: U8 => u.u64() + 1000 else 7 end
class D is T
  new create() => None
  fun run(f: {(U8): U8} val): U64 => f(255).u64()
actor Main
  new create(env: Env) =>
    env.out.print(C.go().string())
    env.out.print(D.go().string())
