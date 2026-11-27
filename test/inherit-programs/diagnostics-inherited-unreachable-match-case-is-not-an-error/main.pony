// I[(U8 | None)]'s f() matches (A | None) with a case for A and one for None.
// In C's copy A already includes None, so the None case can't be reached.
// Expects the program to compile and print 1: the unreachable case is in
// inherited code, which is not reported.
interface I[A: Any val]
  fun f(x: (A | None)): U8 =>
    match \exhaustive\ x
    | let a: A => 1
    | None => 0
    end
class C is I[(U8 | None)]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(None).string())
