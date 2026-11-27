// A generic trait's literal has a match with an unreachable case, and G[A],
// declared after the trait, provides it. The test expects the unreachable
// case reported once, at the literal's text, with no repeat for G's copy.
trait T[A: Any val]
  fun m(x: (A | None)): T[A] ref =>
    object ref is T[A]
      fun k(y: U8): String =>
        match y
        | let u: U8 => "u"
        | let v: U8 => "v"
        end
    end
  fun k(y: U8): String => "t"
class G[A: Any val] is T[A]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(G[U8].m(None).k(1))
