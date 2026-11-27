// Bar fills A with String, so f()'s iftype A <: U8 branch can't run, and the
// else branch is the literal 7. In Bar's copy the if is (String | U8), not U8.
// Expects "function body isn't the result type" with a note naming method 'f'
// inherited by Bar from Foo.
trait Foo[A: Any val]
  fun f(x: A): U8 =>
    iftype A <: U8 then x else 7 end
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f("s").string())
