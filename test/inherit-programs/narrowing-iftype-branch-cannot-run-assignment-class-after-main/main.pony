// Baz fills A with (P | None), so f()'s iftype branch that assigns mk() to a
// Stringable can't run in Baz's copy, where mk() returns a P. Expects "other":
// the assignment in that branch is not an error. Baz is declared after Main.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let s: Stringable val = mk()
      "s"
    else "other" end
actor Main
  new create(env: Env) =>
    let o = Baz
    env.out.print(o.f())
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
