// V.f's default literal reads V's type parameter A in an iftype. Calling f
// on C and on a V[U8] must both use A = U8 and print u8 u8.
interface Named
  fun name(): String
trait V[A: Any val]
  fun f(x: Named val = object val fun name(): String => iftype A <: U8 then "u8" else "other" end end): String => x.name()
class C is V[U8]
  new create() => None
actor Main
  new create(env: Env) =>
    let v: V[U8] = C
    env.out.print(C.f() + " " + v.f())
