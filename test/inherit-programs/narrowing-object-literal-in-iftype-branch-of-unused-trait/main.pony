// U.make builds an object literal of T[Array[A] val] inside iftype A <: U8.
// Nothing inherits U. The program must compile and print x.
trait T[A: Any val]
  fun depth(): USize => 0

trait U[A: Any val]
  fun make(): USize =>
    iftype A <: U8 then
      let inner: T[Array[A] val] val = object val is T[Array[A] val] end
      inner.depth()
    else
      0
    end

actor Main
  new create(env: Env) =>
    env.out.print("x")
