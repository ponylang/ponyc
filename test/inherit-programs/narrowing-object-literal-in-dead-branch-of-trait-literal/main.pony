// T.m returns an object literal that is T2[String]. Its copy of q() holds
// another object literal in an iftype branch that cannot run for String. It
// must compile; expects o.
trait T2[A: Any val]
  fun q(): String =>
    iftype A <: U8 then (object val fun s(): String => "u8" end).s() else "o" end
trait T
  fun m(): T2[String] val => object val is T2[String] end
class C is T
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.m().q())
