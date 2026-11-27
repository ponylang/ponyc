// T.g returns a lambda typed by g's result. Expects C.g(), D.g() and a call
// through T to give the same object: every copy of g shares one lambda class.
trait T
  fun g(): {(): U8} val => {(): U8 => 3}
class val C is T
  new val create() => None
class val D is T
  new val create() => None
actor Main
  new create(env: Env) =>
    let t: T val = C
    env.out.print((C.g() is D.g()).string() + " " + (C.g() is t.g()).string() + " " + (C.g() is C.g()).string())
