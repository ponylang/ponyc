// Q[B2] provides Foo[U8, B2], so f()'s iftype A <: Array[B] branch can't run
// for any B2. Expects "else": U8 having no size() is not an error in that
// branch of Q's copy.
trait Foo[A: Any val, B: Any val]
  fun f(x: A): String =>
    iftype A <: Array[B] val then x.size().string() else "else" end
class Q[B2: Any val] is Foo[U8, B2]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Q[U8].f(3))
