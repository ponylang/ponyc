// lib's trait T returns an object literal whose string() is its __loc type
// name. Expects the same name, $2$0, from C, D and a call through T, and C's
// and D's results to be the same object: all share lib's one literal class.
use l = "lib"
class val C is l.T
  new val create() => None
class val D is l.T
  new val create() => None
actor Main
  new create(env: Env) =>
    let t: l.T val = C
    env.out.print(C.mk().string() + " " + D.mk().string() + " " + t.mk().string() + " " + (C.mk() is D.mk()).string())
