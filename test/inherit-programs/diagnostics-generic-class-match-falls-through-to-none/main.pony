// The generic-class version: for G[String] the U8 case cannot match, and the
// match falls through to None; expects none.
class G[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    end

class C2
  new create() => None

actor Main
  new create(env: Env) =>
    match G[String].f("x")
    | None => env.out.print("none")
    | let s: String => env.out.print("str " + s)
    end
