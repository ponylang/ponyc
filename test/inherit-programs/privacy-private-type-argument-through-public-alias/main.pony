// p2 names its private _H through the public alias PubH and fills p1's T1
// with it. main's class C inherits T1's say(), which calls name() on the
// value. Expects "p2 private H": the alias is written in p2, so _H is allowed.
use p1 = "p1"
use p2 = "p2"

class C is p2.T2

actor Main
  new create(env: Env) =>
    env.out.print(C.say(p2.Make.h()))
