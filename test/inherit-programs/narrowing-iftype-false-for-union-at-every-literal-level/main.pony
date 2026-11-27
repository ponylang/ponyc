// An object literal provides Foo[(P | None)], not Stringable since P has no
// string(); Foo.w returns literals of Foo[(A | None)]. Expects "other" at all
// three levels.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
  fun w(): Foo[(A | None)] val => object val is Foo[(A | None)] fun mk(): (A | None) => None end
actor Main
  new create(env: Env) =>
    let o = object val is Foo[(P | None)] fun mk(): (P | None) => P end
    env.out.print(o.f() + " " + o.w().f() + " " + o.w().w().f())
