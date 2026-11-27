// For Bar the iftype A <: String branch cannot run. r's type must still
// include that branch, so Show sees more than String; expects wide.
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: String then "exact" else "wide" end
trait Foo[A: Any val]
  fun f(x: A): String =>
    let r = iftype A <: String then match x | let s: String => s end else "e" end
    Show(r)
class Bar is Foo[(String | U8)]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(U8(1)))
