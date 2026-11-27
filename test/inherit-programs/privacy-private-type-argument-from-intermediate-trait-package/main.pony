// p2's trait T2 fills p1's T1 with p2's private _H2, and main's class C
// inherits T1's f, which calls name() on _H2. Expects "p2._H2": the type
// argument was written in p2, so its privacy is judged in p2, not p1 or main.
use p2 = "p2"
class C is p2.T2
actor Main
  new create(env: Env) => env.out.print(C.f(p2.MakeH2()))
