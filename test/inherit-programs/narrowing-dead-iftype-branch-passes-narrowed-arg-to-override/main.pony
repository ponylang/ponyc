// Baz's copy of f() has an iftype branch that cannot run for Array[U8] val. The
// branch passes the narrowed x to take(), which Baz overrides. The copy must
// compile; expects other.
trait Foo[A: Any val]
  fun take(a: A): String
  fun f(x: A): String =>
    iftype A <: Stringable val then take(x) else "other" end
class Baz is Foo[Array[U8] val]
  new create() => None
  fun take(a: Array[U8] val): String => "took"
actor Main
  new create(env: Env) => env.out.print(Baz.f([as U8: 1]))
