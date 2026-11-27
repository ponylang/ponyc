// Foo.f captures mk()'s result in a lambda that uses iftype on A. Baz's mk
// returns U64 although A is (U64 | None). Expects "34": each class's lambda
// captures the value with its own type and takes the iftype's then branch.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    let v = mk()
    let g = {()(v): String => iftype A <: Stringable val then v.string() else "other" end} val
    g()
class Bar is Foo[U64]
  new create() => None
  fun mk(): U64 => 3
class Baz is Foo[(U64 | None)]
  new create() => None
  fun mk(): U64 => 4
actor Main
  new create(env: Env) => env.out.print(Bar.f() + Baz.f())
