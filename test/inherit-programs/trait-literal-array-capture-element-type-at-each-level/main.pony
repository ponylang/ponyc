// m captures a = [get()], and C overrides get to return U64, so the array's
// element type differs between C's copy and T's. Both levels must compile
// and report size 1, printing 1 1.
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): T[Array[A] val] ref =>
    let a = [get()]
    object ref is T[Array[A] val]
      fun size(): USize => a.size()
    end
  fun size(): USize => 0
class C is T[U8]
  new create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) =>
    env.out.print(C.m().size().string() + " " + C.m().m().size().string())
