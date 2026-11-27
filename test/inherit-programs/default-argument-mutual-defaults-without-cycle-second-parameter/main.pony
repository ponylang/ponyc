// f's default calls g with one argument, so g's second parameter uses its
// default, and g's first default calls f(). No default depends on itself;
// the program must compile and print 5 6.
primitive P
  fun f(x: U8 = P.g(1)): U8 => x
  fun g(y: U8 = P.f(), z: U8 = 2): U8 => y + z
actor Main
  new create(env: Env) => env.out.print(P.f(5).string() + " " + P.g(4).string())
