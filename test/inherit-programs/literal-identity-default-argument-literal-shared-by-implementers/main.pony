// T's default argument is an object literal with no captures. Expects the
// default from C.f() and from D.f() to be the same object: both classes share
// one class for the literal.
trait T
  fun f(x: Any val = object val end): Any val => x
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C.f() is D.f()).string() + " " + (C.f() is C.f()).string())
