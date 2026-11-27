// An object literal provides T[U16, U16, U16], and T's f0, f1, f2 each return
// a literal with one type argument replaced by U8. Expects "t" after a chain
// of four calls: the literal classes compile for every mix of arguments.
trait T[A0: Any val, A1: Any val, A2: Any val]
  fun name(): String => "t"
  fun f0(): T[U8, A1, A2] val => object val is T[U8, A1, A2] end
  fun f1(): T[A0, U8, A2] val => object val is T[A0, U8, A2] end
  fun f2(): T[A0, A1, U8] val => object val is T[A0, A1, U8] end
actor Main
  new create(env: Env) =>
    let o = object val is T[U16, U16, U16] end
    env.out.print(o.f0().f1().f2().f0().name())
