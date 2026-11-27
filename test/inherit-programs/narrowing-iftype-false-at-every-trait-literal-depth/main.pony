// Foo[A].f() calls mk().string() only when A is Stringable. Baz is
// Foo[(P | None)] and P is not Stringable, so the literals from Baz.w(),
// Baz.w().w(), and Baz.w().w().w() must each print "other" without the
// branch that can't run being checked against their A.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(A | None)] val => object val is Foo[(A | None)] fun mk(): (A | None) => None end
class Baz is Foo[(P | None)]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.w().f() + " " + Baz.w().w().f() + " " + Baz.w().w().w().f())
