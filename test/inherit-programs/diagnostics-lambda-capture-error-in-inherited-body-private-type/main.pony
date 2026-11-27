// A lambda in lib's trait captures `this` as `me` and calls foo(), whose
// type is the private _L. Expects one error, "right side must be a subtype of
// left side" at the lambda, with a note naming method 'bar' inherited by C
// from T. The privacy of _L itself is not reported as an error.
use l = "lib"
class C is l.T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar())
