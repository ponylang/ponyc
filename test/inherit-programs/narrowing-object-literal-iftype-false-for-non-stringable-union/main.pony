// An object literal provides Foo[(P | None)]; P has no string(), so the union
// is not Stringable. Expects "other" from the inherited iftype (#6735).
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String => iftype A <: Stringable val then mk().string() else "other" end
actor Main
  new create(env: Env) =>
    let o = object is Foo[(P | None)] fun mk(): P => P end
    env.out.print(o.f())
