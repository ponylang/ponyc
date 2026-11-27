// A generic method matches (A | B), with A val and B ref, against A, which
// could need a runtime capability check. The program must fail with that
// error once.
class Foo
  var v: U8 = 0
  new create() => None
primitive K
  fun f[A: Any val, B: Any ref](x: (A | B)): String =>
    match x
    | let y: A => "a"
    else "other" end
actor Main
  new create(env: Env) => env.out.print(K.f[Foo val, Foo ref](Foo))
