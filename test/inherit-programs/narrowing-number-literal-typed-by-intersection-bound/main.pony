// K.g's type parameter A is bounded by (Any val & U8 val), so the literal 3
// can be typed A. K.g[U8]() must print 3.
primitive K
  fun g[A: (Any val & U8 val)](): String => let y: A = 3; y.string()
actor Main
  new create(env: Env) => env.out.print(K.g[U8]())
