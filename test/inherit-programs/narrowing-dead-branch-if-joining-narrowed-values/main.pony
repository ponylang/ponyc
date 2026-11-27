// Baz inherits f with A = P, which is not Stringable, so the iftype's then
// branch can't run. An if that joins two mk() values in that branch, followed
// by a string() call, must not be an error; the program prints other.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(b: Bool): String =>
    iftype A <: Stringable val then
      let x = if b then mk() else mk() end
      x.string()
    else "other" end
class Baz is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f(true))
