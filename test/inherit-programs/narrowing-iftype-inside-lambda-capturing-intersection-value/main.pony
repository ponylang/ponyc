// A lambda in G[A].f captures v, of type (P & A), and runs an iftype on A.
// For G[(P | None)], A is not Stringable, so the lambda returns other.
primitive P
class G[A: Any val]
  fun f(v: (P & A)): String =>
    let g = {()(v): String => iftype A <: Stringable val then v.string() else "other" end} val
    g()
actor Main
  new create(env: Env) => env.out.print(G[(P | None)].f(P))
