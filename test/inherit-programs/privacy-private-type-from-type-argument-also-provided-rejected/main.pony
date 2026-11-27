// C provides lib's T[l2.Thing] and also l2's Thing, whose get() returns l2's
// private _Y. T's bar, written in lib, calls y() on what A's get() returns.
// Providing Thing doesn't lead to T, so lib can't look up methods on _Y: the
// test expects the private-type error, blaming main's type argument.
use l = "lib"
use l2 = "l2"
class C is (l.T[l2.Thing] & l2.Thing)
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(l2.Impl))
