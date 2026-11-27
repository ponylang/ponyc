// Foo[A].f() narrows with iftype A <: Stringable val, and Foo.w() returns a
// Foo[(A | None)] literal. No class provides Foo; the program must compile
// and print "ok".
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(A | None)] val => object val is Foo[(A | None)] fun mk(): (A | None) => None end
actor Main
  new create(env: Env) => env.out.print("ok")
