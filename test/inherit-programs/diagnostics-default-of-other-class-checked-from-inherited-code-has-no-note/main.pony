// C's copy of T's f calls D.g(), whose default argument is a String for a U8.
// That default is checked when C's copy needs it, but it is D's code, not
// inherited code, so the test expects its error with no inherited-by note.
class C is T
  new create() => None
trait T
  fun f(): U8 => D.g()
class D
  new create() => None
  fun g(x: U8 = "not a U8"): U8 => x
actor Main
  new create(env: Env) => env.out.print(C.f().string())
