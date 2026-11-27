// C fills A with P, which is not Stringable, so f()'s iftype branch that
// calls mk().string() and passes the clone as a String iso can't run in C's
// copy. Expects "other": the code in that branch is not an error.
primitive P
class Box
  fun ref take(s: String iso): String val => consume s
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let b = Box
      b.take(mk().string().clone())
    else
      "other"
    end
class C is Foo[P]
  new create() => None
  fun mk(): P => P
actor Main
  new create(env: Env) =>
    env.out.print(C.f())
