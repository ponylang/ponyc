// C is T[(U8 | None)], so in C's copy the None case comes after a case that
// already matches None. The \exhaustive\ match must not be reported as an
// error; expects 1.
trait T[A: Any val]
  fun f(x: (A | None)): U8 =>
    match \exhaustive\ x
    | let a: A => 1
    | None => 0
    end
class C is T[(U8 | None)]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(None).string())
