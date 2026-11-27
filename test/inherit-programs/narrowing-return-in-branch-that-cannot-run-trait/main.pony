// Foo's f returns a from an iftype branch that narrows A to U8. Bar gives
// String for A, so that branch can't run in Bar's copy; Foo's own check
// accepted the return under the narrowing. The test expects Bar's copy to
// compile too and f to return 0.
trait Foo[A: Any val]
  fun f(a: A): U8 =>
    iftype A <: U8 val then
      return a
    end
    0

class Bar is Foo[String]
  new create() => None

actor Main
  new create(env: Env) => env.out.print(Bar.f("s").string())
