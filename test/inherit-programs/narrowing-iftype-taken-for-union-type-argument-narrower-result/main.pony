// Foo.f calls mk().string() inside iftype A <: Stringable. Baz has A = P and
// Qux has A = (P | U8) but its mk returns U8. Both types are Stringable, so
// each copy takes the iftype branch and prints "P" and "4".
primitive P
  fun string(): String iso^ => "P".clone()
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
class Baz is Foo[P]
  new create() => None
  fun mk(): P => P
class Qux is Foo[(P | U8)]
  new create() => None
  fun mk(): U8 => 4
actor Main
  new create(env: Env) => env.out.print(Baz.f()); env.out.print(Qux.f())
