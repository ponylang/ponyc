// T[A].depth() builds a T[Array[A] val] literal and calls its depth(), but
// only when A is U8. C is T[U8], so one literal is built, its A is not U8,
// and the program prints 1. The literal's growing type must not make the
// compiler expand T forever.
trait T[A: Any val]
  fun depth(): USize =>
    iftype A <: U8 then
      // one level of wrapping: only taken when A is U8
      let inner: T[Array[A] val] = object ref is T[Array[A] val] end
      1 + inner.depth()
    else
      0
    end

class C is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C.depth().string())
