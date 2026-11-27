// f's default argument is an empty Array[A]. Calling f without an argument on
// G[U8] and on G[String] must type the default with each A; both print 0.
class G[A: Any val]
  new create() => None
  fun f(x: Array[A] val = recover val Array[A] end): USize => x.size()
actor Main
  new create(env: Env) => env.out.print(G[U8].f().string() + G[String].f().string())
