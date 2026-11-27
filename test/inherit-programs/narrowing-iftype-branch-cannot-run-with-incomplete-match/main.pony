// Bar fills In with U8, so f()'s iftype In <: String branch, whose match on x
// has only a String case, can't run in Bar's copy. Expects "other": the
// match there not covering U8 is not an error.
trait Foo[In: Any val]
  fun f(x: In): String =>
    iftype In <: String then
      match x | let s: String => s end
    else "other" end
class Bar is Foo[U8]
actor Main
  new create(env: Env) => env.out.print(Bar.f(1))
