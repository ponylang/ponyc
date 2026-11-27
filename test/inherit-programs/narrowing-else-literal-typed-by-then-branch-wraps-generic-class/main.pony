// In G[String].f the iftype's then branch is U8(2), so the else branch's 255 is
// a U8 too. A is String, so x is 255 and x + 1 wraps to 0, which is printed.
class G[A]
  new create() => None
  fun f(): String =>
    let x = iftype A <: U8 then U8(2) else 255 end
    (x + 1).string()
actor Main
  new create(env: Env) => env.out.print(G[String].f())
