// For G[String] the literal 1 is a U32 because of the iftype branch that cannot
// run, so x - 2 wraps around. Expects 4294967295.
class G[A]
  new create() => None
  fun f(): String =>
    let x = iftype A <: U8 then U32(2) else 1 end
    (x - 2).string()
actor Main
  new create(env: Env) => env.out.print(G[String].f())
