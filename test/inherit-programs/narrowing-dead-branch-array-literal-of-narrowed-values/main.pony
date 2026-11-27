// Baz inherits f with A = P, which is not Stringable, so the iftype's then
// branch can't run. An array literal of mk() values in that branch, whose
// elements call string(), must not be an error; the program prints other.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let xs = [mk(); mk()]
      var s = String
      for x in xs.values() do s.append(x.string()) end
      s.clone()
    else "other" end
class Baz is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f())
