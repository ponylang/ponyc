// T[A].depth() builds a U literal only when A is U8, and U.go() builds a T
// literal, so each trait's default body makes an object of the other trait.
// C is T[U8]: the literals nest one level and the program prints 1. The
// classes for both literals must be finished without recursing forever.
trait T[A: Any val]
  fun depth(): USize =>
    iftype A <: U8 then
      // wrap once: only reachable when A is U8
      let u: U[Array[A] val] = object ref is U[Array[A] val] end
      1 + u.go()
    else
      0
    end

trait U[B: Any val]
  fun go(): USize =>
    let t: T[B] = object ref is T[B] end
    t.depth()

class C is T[U8]

actor Main
  new create(env: Env) =>
    env.out.print(C.depth().string())
