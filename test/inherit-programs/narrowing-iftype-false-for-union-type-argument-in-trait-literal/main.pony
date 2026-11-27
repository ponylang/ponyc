// Foo[A].f() calls mk().foo() only when A is a HasFoo. C is Foo[U8], and
// C.w() and C.w().w() return Foo[(U8 | None)] literals, which are not
// HasFoo, so each f() must print "other". The branch that can't run must not
// be checked against the literal's A.
interface HasFoo
  fun foo(): String
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: HasFoo val then mk().foo() else "other" end
  fun w(): Foo[(A | None)] val => object val is Foo[(A | None)] fun mk(): None => None end
class val C is Foo[U8]
  new val create() => None
  fun mk(): U8 => 1
actor Main
  new create(env: Env) => env.out.print(C.w().f() + " " + C.w().w().f())
