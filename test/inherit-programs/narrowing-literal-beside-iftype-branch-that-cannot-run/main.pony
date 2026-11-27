// C fills A with String, so f()'s iftype A <: U8 branch that returns mk()
// can't run, and x takes its type from the literal 1 in the else branch.
// Expects the program to compile and print 1.
trait Foo[A: Any val]
  fun mk(): (U8 | String)
  fun f(): String =>
    let x = iftype A <: U8 then mk() else 1 end
    x.string()
class C is Foo[String]
  new create() => None
  fun mk(): String => "s"
actor Main
  new create(env: Env) => env.out.print(C.f())
