// Trait Foo's f matches x against U8 and String with no else. Class C is
// Foo[U8] and inherits f; C.f(3) must take the U8 case and print u.
trait Foo[A: (U8 val | String val)]
  fun f(x: A): (String | None) =>
    match x
    | let u: U8 => "u"
    | let s: String => s
    end

class C is Foo[U8]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.f(3).string())
