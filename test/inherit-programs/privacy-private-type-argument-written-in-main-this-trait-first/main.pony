// C provides (p1.T1[p2.PubH] & p2.T2), and main writes the type argument
// p2.PubH, an alias of p2's private _H. Expects the private-type lookup error
// in say() with a note pointing at main's type argument, whatever the order of
// the provided traits.
use p1 = "p1"
use p2 = "p2"

class C is (p1.T1[p2.PubH] & p2.T2)

actor Main
  new create(env: Env) =>
    env.out.print(C.say(p2.Make.h()))
