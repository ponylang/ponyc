// P.f()'s default argument calls P.f() with no argument, so computing the
// default needs the default. The program must fail with one error that the
// default uses itself through default arguments.
primitive P
  fun f(x: U8 = P.f()): U8 => x
actor Main
  new create(env: Env) => env.out.print(P.f(3).string())
