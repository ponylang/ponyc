// An object literal of Foo[String] overrides mk to return (String | U8). The
// inherited f matches mk() against U8 and then A; U8(1) prints u.
trait Foo[A: Any val]
  fun mk(): (A | U8)
  fun f(): String =>
    match mk()
    | let u: U8 => "u"
    | let a: A => "a"
    end
actor Main
  new create(env: Env) =>
    let o = object is Foo[String] fun mk(): (String | U8) => U8(1) end
    env.out.print(o.f())
