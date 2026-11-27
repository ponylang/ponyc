// T's two guards test different type parameters, A and B. C[X] gives X for
// both, so in C's copies the guards test the same type, but T's own check
// found no shadowing. The test expects no error and the specializations the
// guards pick: 1 for C[U8], 0 for C[String].
trait T[A: Any val, B: Any val]
  fun f(): U8 => 0
  fun f(): U8 iftype A <: U8 val => 1
  fun f(): U8 iftype B <: U8 val => 2

class C[X: Any val] is T[X, X]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C[U8].f().string() + " " + C[String].f().string())
