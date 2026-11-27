// Foo.f's lambda takes a parameter of type A and uses iftype on A. Baz
// provides Foo[(P | None)], which is not Stringable because P has no string().
// Expects "other".
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    let g = {(a: A): String =>
      iftype A <: Stringable val then a.string() else "other" end}
    g(mk())
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
