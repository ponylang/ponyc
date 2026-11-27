// A class method's default argument calls another method on the same object.
// Default arguments can't refer to `this`, so the test expects that error.
class C
  fun n(): U8 => 3
  fun f(x: U8 = n()): U8 => x
actor Main
  new create(env: Env) => env.out.print(C.f().string())
