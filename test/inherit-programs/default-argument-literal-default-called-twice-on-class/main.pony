// T.f's default is an object literal. Calling f() twice on C directly must
// build the default both times; expects lit, lit.
interface val S
  fun s(): String
trait T
  fun f(x: S = object val fun s(): String => "lit" end): String => x.s()
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f()); env.out.print(C.f())
