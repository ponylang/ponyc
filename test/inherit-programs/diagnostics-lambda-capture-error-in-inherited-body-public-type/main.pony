// A lambda in lib's trait captures `this` as `me`, and in C's copy `me` is a
// C box where C ref is needed. Expects "right side must be a subtype of left
// side" with a note naming method 'bar' inherited by C from T.
use l = "lib"
class C is l.T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.bar())
