// T's default argument is an object literal of T. Expects the default from
// C.f() and from a call through T val to be the same object: C's copy and the
// trait share one class for the literal.
trait T
  fun f(x: T val = object val is T end): T val => x
class val C is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let t: T val = C
    env.out.print((C.f() is t.f()).string() + " " + (C.f() is C.f()).string())
