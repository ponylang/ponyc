// Q[B2] provides Foo[Array[U8] val, B2], so f()'s iftype A <: Array[B] branch
// runs only when B2 is U8. Expects "9" for Q[U8] and "s" for Q[String], where
// the else branch returns b.
trait Foo[A: Any val, B: Any val]
  fun f(a: A, b: B): B =>
    iftype A <: Array[B] val then try a(0)? else b end else b end
class Q[B2: Any val] is Foo[Array[U8] val, B2]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].f([as U8: 9], 1).string())
    env.out.print(Q[String].f([as U8: 9], "s"))
