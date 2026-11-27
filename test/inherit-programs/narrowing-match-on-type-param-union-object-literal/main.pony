// Foo.f matches None, held in an (A | None), against A. An object literal of
// Foo[U8] inherits f, and the match must fall to else and print none.
trait Foo[A: Any val]
  fun f(): String =>
    let x: (A | None) = None
    match x
    | let a: A => "a"
    else "none" end
actor Main
  new create(env: Env) =>
    let o = object is Foo[U8] end
    env.out.print(o.f())
