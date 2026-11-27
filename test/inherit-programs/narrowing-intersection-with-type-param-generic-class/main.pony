// G's parameters are intersections of a primitive and the type parameter A.
// Inside iftype A <: Stringable, x.string() can be called. (P | None) is not
// Stringable, so f prints other; P2 and U8 are, so g prints p2 and h prints 5.
primitive P
primitive P2
  fun string(): String iso^ => "p2".clone()

class G[A: Any val]
  fun f(x: (P & A)): String =>
    iftype A <: Stringable val then x.string() else "other" end
  fun g(x: (P2 & A)): String =>
    iftype A <: Stringable val then x.string() else "other" end
  fun h(x: (Any val & A)): String =>
    iftype A <: Stringable val then x.string() else "other" end

actor Main
  new create(env: Env) =>
    env.out.print(G[(P | None)].f(P))
    env.out.print(G[P2].g(P2))
    env.out.print(G[U8].h(U8(5)))
