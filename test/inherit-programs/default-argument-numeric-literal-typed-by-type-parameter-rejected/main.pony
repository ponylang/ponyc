// G[A].f()'s default 255 and the 1 in its body are numeric literals typed as
// A, an Integer[A] val. The program must fail with "could not infer literal
// type" once at each literal, with no further errors from the calls on
// G[U8] and G[U16].
class G[A: Integer[A] val]
  fun f(x: A = 255): A => x + 1
actor Main
  new create(env: Env) =>
    env.out.print(G[U8].f().string() + " " + G[U16].f().string())
