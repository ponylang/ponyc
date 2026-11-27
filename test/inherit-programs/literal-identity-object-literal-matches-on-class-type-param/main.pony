// K[A].m returns an object literal whose method matches a U8 against A.
// K[U8] and K[String] must each get a literal that uses its own A, printing
// match and no.
interface val G
  fun g(): String
class K[A: Any val]
  new create() => None
  fun m(): G =>
    object val
      fun g(): String =>
        let v: Any val = U8(1)
        match v | let a: A => "match" else "no" end
    end
actor Main
  new create(env: Env) => env.out.print(K[U8].m().g() + " " + K[String].m().g())
