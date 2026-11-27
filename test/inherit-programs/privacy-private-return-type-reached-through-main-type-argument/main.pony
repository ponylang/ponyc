// main fills lib's T with l2.Thing, whose get() returns l2's private _Y, so
// the inherited bar() calls y() on _Y. Expects the private-type lookup error
// with a note pointing at main's type argument, written outside l2.
use l = "lib"
use l2 = "l2"
class C is l.T[l2.Thing]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar(l2.Thing))
