// T.f's default is an object literal. Calling f() on C through a T must build
// the default; expects lit.
interface val S
  fun s(): String
trait T
  fun f(x: S = object val fun s(): String => "lit" end): String => x.s()
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    let t: T = C
    env.out.print(t.f())
