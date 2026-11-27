// p2's T2 fills p1's T1 with p1.PubH1, a public alias of p1's private _H1,
// so the inherited f() calls name() on _H1 through a type argument written in
// p2. Expects the private-type lookup error for T2 and for C, each with a note
// pointing at that type argument.
use p1 = "p1"
use p2 = "p2"
class C is p2.T2
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(p1.MakeH1()))
