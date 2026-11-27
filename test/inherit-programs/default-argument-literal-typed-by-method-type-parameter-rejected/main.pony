// A numeric literal can't take its type from a method type parameter. Expects
// one error for the literal in the default argument and one in the body, and
// no further error at the calls that use the default.
primitive P
  fun f[A: Integer[A] val](x: A = 255): A => x + 1
actor Main
  new create(env: Env) =>
    env.out.print(P.f[U8]().string() + " " + P.f[U16]().string())
