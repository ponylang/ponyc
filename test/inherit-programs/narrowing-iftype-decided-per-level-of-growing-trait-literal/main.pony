// An object literal provides Foo[U8]; Foo.w returns a literal of
// Foo[(Array[A] val | None)]. Expects "3 other other": the inherited iftype is
// decided for each level's own type argument.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(Array[A] val | None)] val => object val is Foo[(Array[A] val | None)] fun mk(): (Array[A] val | None) => None end
actor Main
  new create(env: Env) =>
    let o = object val is Foo[U8] fun mk(): U8 => 3 end
    env.out.print(o.f() + " " + o.w().f() + " " + o.w().w().f())
