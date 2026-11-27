// P.f's default argument is written on line 32 of p.pony and is used by a
// call in main.pony. The test checks only that the program prints 8.
actor Main
  new create(env: Env) =>
    env.out.print(P.f().string())
