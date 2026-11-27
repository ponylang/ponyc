// C fills A with String, so f()'s iftype A <: U8 branch, which assigns the
// literal 3 to a local of type A, can't run in C's copy. Expects "o": the
// literal having no valid type in that branch is not an error.
trait Foo[A: Any val]
  fun f(): String =>
    iftype A <: U8 val then let y: A = 3; y.string() else "o" end
class C is Foo[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f())
