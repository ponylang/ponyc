// G[A].f assigns x to v: A inside iftype A <: Stringable, then calls
// v.string(). G[U8] must print 4.
class G[A: Any val]
  fun f(x: A): String =>
    var v: A = x
    iftype A <: Stringable val then v = x; v.string() else "other" end
actor Main
  new create(env: Env) => env.out.print(G[U8].f(U8(4)))
