// Trait Foo's f matches x against U8 and String with no else. An object
// literal of Foo[U8] inherits f; f(3) must take the U8 case and print u.
trait Foo[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    | let s: String => s
    end

actor Main
  new create(env: Env) =>
    let o = object is Foo[U8] end
    env.out.print(o.f(3).string())
