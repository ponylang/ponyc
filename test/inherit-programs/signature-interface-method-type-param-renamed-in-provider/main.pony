// I1.f names its type parameter A and C2.f names it B. C2 must still be an I1,
// and a call through I1 must reach C2's f; expects 3 (#6733).
interface I1
  fun f[A: Stringable val](x: A): String

class C2
  fun f[B: Stringable val](x: B): String => x.string()

actor Main
  new create(env: Env) =>
    let i: I1 = C2
    env.out.print(i.f[U8](3))
