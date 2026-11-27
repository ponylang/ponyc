// P.f()'s default calls Q.g() and Q.g()'s default calls P.f(). The program
// must fail with one error that f()'s default uses itself through default
// arguments, pointing at both defaults.
primitive P
  fun f(x: U8 = Q.g()): U8 => x
primitive Q
  fun g(y: U8 = P.f()): U8 => y
actor Main
  new create(env: Env) => env.out.print(P.f(3).string())
