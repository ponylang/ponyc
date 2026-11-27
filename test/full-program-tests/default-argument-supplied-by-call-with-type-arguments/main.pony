// f's default calls g with no arguments, and g's default calls f with 7 for
// x, so g's default doesn't need f's. The calls have type arguments. The
// program must compile, not report a default that uses itself, and print 7.
primitive P
  fun f[A: Any val](x: U8 = P.g[U8]()): U8 => x
  fun g[A: Any val](y: U8 = P.f[U8](7)): U8 => y
actor Main
  new create(env: Env) => env.out.print(P.g[U8]().string())
