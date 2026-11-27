// For G[P] the iftype Stringable branch cannot run, and its result is joined
// with a String and assigned to Stringable val. Must compile; expects s.
primitive P
class G[A: Any val]
  new create() => None
  fun f(x: A): String =>
    let v = iftype A <: Stringable val then x else "s" end
    let w: Stringable val = v
    w.string()
actor Main
  new create(env: Env) => env.out.print(G[P].f(P))
