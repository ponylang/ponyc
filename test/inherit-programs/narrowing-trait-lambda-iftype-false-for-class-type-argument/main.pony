// Foo.f's lambda captures v: A and uses iftype on A. Baz provides
// Foo[Array[U8] val], which is not Stringable. Expects "other".
trait Foo[A: Any val]
  fun f(v: A): String =>
    let g = {()(v): String => iftype A <: Stringable val then v.string() else "other" end} val
    g()
class Baz is Foo[Array[U8] val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Baz.f([as U8: 1]))
