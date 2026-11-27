// Foo[A].w() returns a Foo[(A | None)] literal whose mk() returns None. f()
// checks iftype A <: Stringable val. Baz is Foo[U8] and prints "3"; the
// literal and the literal's own literal are Foo[(U8 | None)], which is
// Stringable, so both print "None". Each copy must narrow with its own A.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(A | None)] val => object val is Foo[(A | None)] fun mk(): (A | None) => None end
class val Baz is Foo[U8]
  new val create() => None
  fun mk(): U8 => 3
actor Main
  new create(env: Env) => env.out.print(Baz.f() + " " + Baz.w().f() + " " + Baz.w().w().f())
