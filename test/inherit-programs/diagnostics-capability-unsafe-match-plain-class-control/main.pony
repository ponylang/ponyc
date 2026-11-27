// A plain class matches (String val | String ref) against String val, which
// could need a runtime capability check. The program must fail with that
// error once.
class C
  new create() => None
  fun f(x: (String val | String ref)): String =>
    match x
    | let s: String val => "val " + s
    else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(C.f(String.>append("r")))
