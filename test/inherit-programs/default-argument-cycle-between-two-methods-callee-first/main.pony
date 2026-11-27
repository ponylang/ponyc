// P.g()'s default calls P.f() and P.f()'s default calls P.g(), with g()
// declared first. The program must fail with one error that g()'s default
// uses itself through default arguments, pointing at both defaults.
primitive P
  fun g(y: U8 = P.f()): U8 => y
  fun f(x: U8 = P.g()): U8 => x
actor Main
  new create(env: Env) => env.out.print(P.f(3).string())
