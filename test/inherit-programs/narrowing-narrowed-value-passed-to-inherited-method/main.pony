// Inside iftype A <: Stringable, Foo.f passes x to take(y: A). Bar is
// Foo[Array[U8] val], which is not Stringable, so f prints other.
trait Foo[A: Any val]
  fun take(y: A): String => "took"
  fun f(x: A): String =>
    iftype A <: Stringable val then take(x) else "other" end
class Bar is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f([as U8: 1]))
