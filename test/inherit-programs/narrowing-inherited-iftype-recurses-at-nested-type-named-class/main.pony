// T.depth recurses through W[Array[A] val] only when A is U8. C is T[U8], so
// the first call recurses once; Array[U8] val is not U8, so the second stops.
// The program prints 1.
trait T[A: Any val]
  fun depth(): USize =>
    iftype A <: U8 then
      let inner: T[Array[A] val] = W[Array[A] val]
      1 + inner.depth()
    else
      0
    end
class W[B: Any val] is T[B]
  new create() => None
class C is T[U8]
actor Main
  new create(env: Env) =>
    env.out.print(C.depth().string())
