// Foo[A].w() returns L[A], an object that is a Foo[(A | None)]. Calling w() on
// that result gives Foo[((A | None) | None)], which is the same type as
// Foo[(A | None)]. The compiler must treat it as the same type and not reach
// ever larger instantiations. The program must compile and print "other other".
primitive P
trait val Foo[A: Any val]
  fun f(): String => "other"
  fun w(): Foo[(A | None)] val => L[A]
class val L[A: Any val] is Foo[(A | None)]
  new val create() => None
class val Baz is Foo[(P | None)]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Baz.w().f() + " " + Baz.w().w().f())
