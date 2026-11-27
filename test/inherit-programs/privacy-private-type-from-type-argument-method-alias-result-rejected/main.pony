// lib's T calls y() on what A's get() returns. main gives l2.Thing for A,
// whose get() returns l2's private _Y through l2's public alias PubY. lib
// can't look up methods on l2's private type, so the test expects the
// private-type error, blaming main's type argument.
use l = "lib"
use l2 = "l2"
class C is l.T[l2.Thing]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(l2.Thing))
