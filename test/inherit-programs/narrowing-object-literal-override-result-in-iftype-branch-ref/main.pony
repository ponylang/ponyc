// A generic class returns an object literal of Foo[(B2 | None)] whose mk
// returns None, which has string(). Expects "None" for U8 and "other" for
// Array[U8] val, which is not Stringable. The method returns a ref.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
class Q[B2: Any val]
  fun make(): Foo[(B2 | None)] =>
    object ref is Foo[(B2 | None)] fun mk(): None => None end
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8].make().f())
    env.out.print(Q[Array[U8] val].make().f())
