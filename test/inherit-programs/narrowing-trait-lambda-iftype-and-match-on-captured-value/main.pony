// T.m returns a lambda that captures get()'s result, uses iftype on A, and
// matches the value. C provides T[U8] and its get returns U64 5. Expects "5".
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): {(): USize} val =>
    let a: Array[U64] val = [1; 2]
    let b = get()
    {()(b): USize => iftype A <: U8 then match b | let u: U64 => u.usize() else 0 end else 0 end} val
class C is T[U8]
  new create() => None
  fun get(): U64 => 5
actor Main
  new create(env: Env) => env.out.print(C.m()().string())
