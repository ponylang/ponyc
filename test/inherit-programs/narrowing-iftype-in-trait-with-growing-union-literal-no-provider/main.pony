// Foo[A].f() narrows with iftype A <: Stringable val, and Foo.w() returns a
// Foo[(Array[A] val | None)] literal. No class provides Foo; the program must
// compile and print "ok": the trait type checks as ordinary generic code.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(Array[A] val | None)] val => object val is Foo[(Array[A] val | None)] fun mk(): (Array[A] val | None) => None end
actor Main
  new create(env: Env) => env.out.print("ok")
