// Foo.f builds an object literal capturing v: A only in the branch where A is
// Stringable. Q[B2] provides Foo[B2]. Expects "narrowed" for Q[U8] and "other"
// for Q[Array[U8] val].
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then
      let v: A = mk()
      let o = object val fun g(): A => v end
      "narrowed"
    else "other" end
class Q[B2: Any val] is Foo[B2]
  let _b: B2
  new create(b: B2) => _b = b
  fun mk(): B2 => _b
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).f())
