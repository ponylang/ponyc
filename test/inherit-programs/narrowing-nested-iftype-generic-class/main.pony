// G.f nests iftype A <: ReadSeq[U8] inside iftype A <: Stringable. U8 is
// Stringable but not a ReadSeq, so G[U8].f prints s. G is a class.
class G[A: Any val]
  new create() => None
  fun f(x: A): String =>
    iftype A <: Stringable val then
      iftype A <: ReadSeq[U8] val then x.string() + x.size().string() else "s" end
    else "o" end
actor Main
  new create(env: Env) => env.out.print(G[U8].f(3))
