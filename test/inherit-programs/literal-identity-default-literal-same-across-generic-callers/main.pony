// P.f's default is an object literal. Calls through G.g with U8 and U16, and
// a direct call, must all get the same object; expects true true.
actor Main
  new create(env: Env) =>
    env.out.print((G.g[U8]() is G.g[U16]()).string() + " " + (G.g[U8]() is P.f()).string())
primitive G
  fun g[Z: Any val](): Any val => P.f()
primitive P
  fun f(x: Any val = object val end): Any val => x
