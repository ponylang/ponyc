// For C, A is String, so the U8 case in C's copy cannot match and r is None.
// Expects None isnone.
trait Foo[A: (U8 val | String val)]
  fun f(x: A): String =>
    let r = match x
    | let u: U8 => "u"
    end
    r.string() + " " + (if r is None then "isnone" else "notnone" end)

class C is Foo[String]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.f("x"))
