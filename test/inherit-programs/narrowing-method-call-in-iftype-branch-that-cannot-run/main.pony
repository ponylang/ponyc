// Baz fills A with Array[U8], which is not Stringable, so f()'s iftype branch
// that calls x.string() can't run in Baz's copy. Expects "other": the call in
// that branch is not an error.
trait Foo[A: Any val]
  fun take(a: A): String => "took"
  fun f(x: A): String =>
    iftype A <: Stringable val then take(x) + x.string() else "other" end
class Baz is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Baz.f([as U8: 1]))
