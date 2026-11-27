// C provides (p2.T2 & p1.T1[p2.PubH]), and main writes the type argument
// p2.PubH, an alias of p2's private _H. Expects the private-type lookup error
// in say() with a note pointing at main's type argument: T2 supplying the same
// type argument from p2 does not make main's use allowed.
use p1 = "p1"
use p2 = "p2"

class C is (p2.T2 & p1.T1[p2.PubH])

actor Main
  new create(env: Env) =>
    env.out.print(C.say(p2.Make.h()))
