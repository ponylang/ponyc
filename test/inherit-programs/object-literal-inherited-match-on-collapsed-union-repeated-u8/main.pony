// An object literal provides Foo[U8], so (A | U8) is just U8, and its mk
// returns (U8 | U8). Expects "u" from the inherited match, whose second case
// can never match.
trait Foo[A: Any val]
  fun mk(): (A | U8)
  fun f(): String =>
    match mk()
    | let u: U8 => "u"
    | let a: A => "a"
    end
actor Main
  new create(env: Env) =>
    let o = object is Foo[U8] fun mk(): (U8 | U8) => U8(1) end
    env.out.print(o.f())
