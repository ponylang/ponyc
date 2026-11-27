// T.f()'s default argument is an object literal that is a T. Calling f() on
// C through a T reference must use that default and print "n".
trait T
  fun n(): String => "n"
  fun f(x: T val = object val is T end): String => x.n()
class C is T
actor Main
  new create(env: Env) =>
    let t: T = C
    env.out.print(t.f())
