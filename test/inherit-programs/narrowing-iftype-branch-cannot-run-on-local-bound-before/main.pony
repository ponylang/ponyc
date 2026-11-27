// m is bound to mk() before the iftype, and Baz fills A with (P | None), so
// the branch calling m.string() can't run in Baz's copy. Expects "other": P
// having no string() is not an error in that branch.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    let m = mk()
    iftype A <: Stringable val then m.string() else "other" end
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
