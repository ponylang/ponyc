// C provides lib's T and, beside it, l2's H, which gives l2's private _Y to
// l2's G. T's bar, written in lib, calls y() on what A's get() returns, and C
// is its own A, so that is _Y. H's entry doesn't lead to T, so lib can't look
// up methods on _Y: the test expects the private-type error.
use l = "lib"
use l2 = "l2"
class val C is (l.T[C] & l2.H)
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(C))
