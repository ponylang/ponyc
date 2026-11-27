// A method type parameter A is bounded by the class type parameter Z, and f
// returns x as a Z. Calling C[String].f[String] must compile; expects ok.
// Issue #6729.
class C[Z: Stringable val]
  fun f[A: Z](x: A): Z => x

actor Main
  new create(env: Env) =>
    env.out.print(C[String].f[String]("ok"))
