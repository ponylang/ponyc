// A is bounded by (U8 | String). f matches x against U8 and String with no
// else. G[U8].f(3) must take the U8 case and print u.
class G[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    | let s: String => s
    end

actor Main
  new create(env: Env) =>
    env.out.print(G[U8].f(3).string())
