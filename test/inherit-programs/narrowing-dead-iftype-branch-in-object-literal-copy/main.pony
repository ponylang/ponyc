// An object literal in Main is Foo[(P | None)], so its copy's iftype
// Stringable branch cannot run. The copy must compile; expects other.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let s: Stringable val = mk()
      "s"
    else "other" end
actor Main
  new create(env: Env) =>
    let o = object is Foo[(P | None)] fun mk(): P => P end
    env.out.print(o.f())
