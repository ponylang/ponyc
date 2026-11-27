// Baz inherits f with A = P, which is not Stringable, so the iftype's then
// branch can't run. A let whose type is inferred from mk() in that branch,
// followed by a string() call, must not be an error; the program prints other.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let x = mk()
      x.string()
    else "other" end
class Baz is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
