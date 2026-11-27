// l2's H provides lib's T and, beside it, l2's G[_Y] with l2's private _Y.
// C inherits T's bar, written in lib, which calls y() on what A's get()
// returns, and that is _Y. The G[_Y] entry doesn't lead to T, so lib can't
// look up methods on _Y: the test expects the private-type error.
use l2 = "l2"
class val C is l2.H
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(C))
