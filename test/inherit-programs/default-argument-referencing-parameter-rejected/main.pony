// A default argument names an earlier parameter. Defaults can't refer to
// parameters, so the program must fail with that error.
primitive P
  fun f(a: U8, b: U8 = a): U8 => b
actor Main
  new create(env: Env) => env.out.print(P.f(3).string())
