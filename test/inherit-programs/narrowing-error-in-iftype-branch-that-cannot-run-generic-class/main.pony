// G[String].f is partial only because the iftype's then branch raises an
// error. A is String, so the else branch runs and the program prints 0.
class G[A: Any val]
  new create() => None
  fun f(): U8 ? =>
    iftype A <: U8 then error else 0 end
actor Main
  new create(env: Env) => try env.out.print(G[String].f()?.string()) end
