// f's default argument is an object literal whose method runs an iftype on the
// class's type parameter A. G[U8] must print u8 and G[String] other, so each
// instantiation gets a default typed with its own A.
trait Named
  fun name(): String
class G[A: Any val]
  new create() => None
  fun f(x: Named val = object val is Named fun name(): String => iftype A <: U8 then "u8" else "other" end end): String => x.name()
actor Main
  new create(env: Env) => env.out.print(G[U8].f() + " " + G[String].f())
