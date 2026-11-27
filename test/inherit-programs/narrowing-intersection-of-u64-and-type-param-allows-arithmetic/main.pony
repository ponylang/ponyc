// H's parameters are intersections of U64 with A or with (U64 | None), and
// x + 1 must compile through the U64 part. H[U64] prints 4 and 5 on one line,
// then H[(U64 | None)] prints 4.
class H[A: Any val]
  fun f(x: (U64 & A)): U64 => x + 1
  fun g(x: (U64 & (U64 | None))): U64 => x + 1
actor Main
  new create(env: Env) =>
    env.out.print(H[U64].f(U64(3)).string() + H[U64].g(U64(4)).string())
    env.out.print(H[(U64|None)].f(U64(3)).string())
