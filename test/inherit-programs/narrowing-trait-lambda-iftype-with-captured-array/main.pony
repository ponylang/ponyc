// T.m returns a lambda that captures an array of get() and uses iftype on A.
// C provides T[U8] and overrides get with the narrower U64. Expects "1": the
// iftype holds and the captured array has one element.
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): {ref(): USize} ref =>
    let a = [get()]
    {ref()(a): USize => iftype A <: U8 then a.size() else 0 end} ref
class C is T[U8]
  new create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(C.m()().string())
