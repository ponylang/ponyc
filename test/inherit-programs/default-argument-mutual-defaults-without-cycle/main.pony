// f's default calls g with an argument, and g's default calls f(). No
// default depends on itself, so the program must compile and print 1 1.
primitive P
  fun f(x: U8 = P.g(1)): U8 => x
  fun g(y: U8 = P.f()): U8 => y
actor Main
  new create(env: Env) => env.out.print(P.f().string() + " " + P.g().string())
