// In G[String].f the iftype's else branch returns "other", so the code after
// the iftype never runs. The program must compile and print other.
class G[A: Any val]
  new create() => None
  fun f(): String =>
    iftype A <: U8 then
      None
    else
      return "other"
    end
    "u8"
actor Main
  new create(env: Env) => env.out.print(G[String].f())
