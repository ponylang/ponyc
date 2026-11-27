// T.f()'s default argument is a String for a U8 parameter, and C1 and C2
// inherit f(). The program must fail with the error once for T and once for
// each class, each class's copy carrying a note naming that class.
trait T
  fun f(x: U8 = "s"): U8 => x
class C1 is T
class C2 is T
actor Main
  new create(env: Env) => env.out.print(C1.f().string() + C2.f().string())
