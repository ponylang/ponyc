// For G[String] the iftype U32 branch cannot run, but x's type still joins it,
// so the literal 1 is a U32. Expects 1.
class G[A]
  new create() => None
  fun f(): String =>
    let x = iftype A <: U8 then U32(2) else 1 end
    x.string()
actor Main
  new create(env: Env) => env.out.print(G[String].f())
