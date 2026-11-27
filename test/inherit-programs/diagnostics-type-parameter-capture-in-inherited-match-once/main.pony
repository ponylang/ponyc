// A trait default matches an (A | Foo ref) value with `let y: A`, and C
// provides the trait with A = Foo val. Foo ref and Foo val can't be told apart
// at runtime, so the program must fail. The test expects one capability error
// at the trait's text, with the notes written for the trait's parameter A.
class Foo
  new create() => None
trait T[A: Any val]
  fun f(x: (A | Foo ref)): String =>
    match x
    | let y: A => "a"
    else "other" end
class C is T[Foo val]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(Foo))
