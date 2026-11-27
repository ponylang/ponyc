// Q[B2].make returns an object literal of Foo[Array[U8] val, B2]. Its
// inherited f indexes a only when A <: Array[B]: Q[U8] prints 9 and Q[String]
// falls back to b and prints s.
trait Foo[A: Any val, B: Any val]
  fun f(a: A, b: B): B =>
    iftype A <: Array[B] val then try a(0)? else b end else b end
class Q[B2: Any val]
  new create() => None
  fun make(): Foo[Array[U8] val, B2] val => object val is Foo[Array[U8] val, B2] end
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].make().f([as U8: 9], 1).string())
    env.out.print(Q[String].make().f([as U8: 9], "s"))
