// A generic class with an iftype branch that cannot run for Array[U8] val; the
// branch passes the narrowed x to take(). Compiles and prints other. This is
// the generic-class version of the inherited cases next to it.
class G[A: Any val]
  fun take(a: A): String => "took"
  fun f(x: A): String =>
    iftype A <: Stringable val then take(x) else "other" end
actor Main
  new create(env: Env) => env.out.print(G[Array[U8] val].f([as U8: 1]))
