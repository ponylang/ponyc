// An interface default matches (A | String ref) against String val, which
// could need a runtime capability check. C provides it with A = String val.
// The error must be reported once at the interface's code, with no note
// naming C.
interface Foo[A: Any val]
  fun f(x: (A | String ref)): String =>
    match x
    | let s: String val => "val " + s
    else "other" end
class C is Foo[String val]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f(String.>append("r")))
    env.out.print(C.f("v"))
