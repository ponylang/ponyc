// P.f()'s default calls P.g() and P.g()'s default calls P.f(), with f()
// declared first. The program must fail with one error that f()'s default
// uses itself through default arguments, pointing at both defaults.
primitive P
  fun f(x: U8 = P.g()): U8 => x
  fun g(y: U8 = P.f()): U8 => y
actor Main
  new create(env: Env) => env.out.print(P.f(3).string())
