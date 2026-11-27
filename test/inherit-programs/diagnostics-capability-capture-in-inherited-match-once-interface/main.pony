// An interface default matches an `A #read` value with `let s: String val`,
// and C provides it with (String ref | String val). The capture can't be told
// apart by capability, so the program must fail. The test expects that error
// once, at the interface's own text, with no second report for C's copy.
interface Foo[A: Any #read]
  fun f(x: A): String =>
    match x
    | let s: String val => "val " + s
    else "other" end
class C is Foo[(String ref | String val)]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f(String.>append("r")))
