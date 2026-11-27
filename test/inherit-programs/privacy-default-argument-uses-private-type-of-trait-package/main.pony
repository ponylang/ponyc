// lib's trait gives p() a default argument that calls name() on lib's private
// _H, and main calls C.p() with no argument. Expects "lib._H": the default
// argument is written in lib, so lib's private type is allowed in it.
use l = "lib"
class C is l.T
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.p())
