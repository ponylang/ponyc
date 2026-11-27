// Bar fills A with String, so f()'s iftype A <: U8 branch that returns x
// can't run, and the else branch returns U8(7). Expects the program to compile
// and print 7: the branch that can't run does not make the body's type wrong.
trait Foo[A: Any val]
  fun f(x: A): U8 =>
    iftype A <: U8 then x else U8(7) end
class Bar is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f("s").string())
