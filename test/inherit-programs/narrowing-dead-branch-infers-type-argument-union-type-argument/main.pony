// Baz inherits f with A = (P | None), which is not Stringable, so the
// iftype's then branch can't run. Inferring Show's type argument from mk() in
// that branch must not be an error; the program prints other.
primitive P
primitive Show
  fun apply[S: Stringable val](s: S): String => s.string()
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then Show(mk()) else "other" end
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
