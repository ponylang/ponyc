// Baz's copy of f() has an iftype branch that cannot run for Array[U8] val. The
// branch passes the narrowed x to the lambda parameter g. The copy must
// compile; expects other.
primitive Sink
  fun apply(a: Array[U8] val): String => "sunk"
trait Foo[A: Any val]
  fun f(x: A, g: {(A): String} val): String =>
    iftype A <: Stringable val then g(x) else "other" end
class Baz is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Baz.f([as U8: 1], {(a: Array[U8] val): String => "g"}))
