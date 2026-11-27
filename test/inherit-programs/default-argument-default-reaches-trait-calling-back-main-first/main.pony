// P.f's default calls T's m(), whose own default is 1, and T's n() calls
// P.f(). This is not a default cycle and must not be reported as one.
// Main is declared first; expects 1 1.
actor Main
  new create(env: Env) => env.out.print(P.f().string() + " " + C.n().string())
primitive P
  fun f(y: U8 = Mk().m()): U8 => y
primitive Mk
  fun apply(): T val => C
trait T
  fun m(x: U8 = 1): U8 => x
  fun n(): U8 => P.f()
class val C is T
  new val create() => None
