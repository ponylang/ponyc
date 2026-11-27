// Baz's copy of f() has an iftype branch that cannot run for Array[U8] val. The
// branch passes the narrowed x to take(), which Baz also inherits. The copy
// must compile; expects other.
trait Foo[A: Any val]
  fun take(a: A): String => "took"
  fun f(x: A): String =>
    iftype A <: Stringable val then take(x) else "other" end
class Baz is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Baz.f([as U8: 1]))
