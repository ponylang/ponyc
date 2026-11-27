// A generic class constrained to Any #read matches its parameter against
// String val, which could need a runtime capability check. The program must
// fail with that error once.
class G[A: Any #read]
  new create() => None
  fun f(x: A): String =>
    match x
    | let s: String val => "val " + s
    else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(G[(String ref | String val)].f(String.>append("r")))
