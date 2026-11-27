// Baz is Foo[P]; P is not Stringable, so in Baz's copy x is (P | U8) and only
// U8 matches the Stringable case. The else must stay; expects other, then 3.
primitive P
trait Foo[A: Any val]
  fun mk(): A
  fun f(x: (A | U8)): String =>
    match x
    | let s: Stringable val => s.string()
    else
      "other"
    end
class Baz is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) => env.out.print(Baz.f(P)); env.out.print(Baz.f(3))
