// T.f's match has only a U8 case and no else, so its value is None for a
// String. C inherits f: f("s") prints none and f(U8(1)) prints u.
trait T
  fun f(x: (U8 | String)): String =>
    let r = match x | let u: U8 => "u" end
    match r | None => "none" | let s: String => s end
class C is T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f("s")); env.out.print(C.f(U8(1)))
