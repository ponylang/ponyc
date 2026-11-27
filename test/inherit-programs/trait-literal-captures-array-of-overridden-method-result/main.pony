// T.m puts get() in an array and returns a trait literal that captures it. C
// overrides get with the narrower U64, so C's copy of m and the literal's copy
// type the array differently. Expects "1 1": both copies compile and the
// capture works at each level.
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): T[Array[A] val] ref =>
    let a: Array[(U64 | None)] = [get()]
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
