// A generic class matches (A | String ref) against String val, which could
// need a runtime capability check. The program must fail with that error
// once, reported at the class's code.
class G[A: Any val]
  new create() => None
  fun f(x: (A | String ref)): String =>
    match x
    | let s: String val => "val " + s
    else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(G[String val].f(String.>append("r")))
    env.out.print(G[String val].f("v"))
