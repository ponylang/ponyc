// T0.fa returns an object literal of (T1 & M). T1.g passes this to an iftype
// that tests for M. In T1's body this is a T1, not an M, so g prints notM.
// Main and C are declared after the traits.
primitive Name
  fun apply[X](x: X): String => iftype X <: M val then "M" else "notM" end
trait val M
trait T0
  fun val fa(): T1 val => object val is (T1 & M) end
trait T1
  fun val g(): String => Name(this)
// isect_depth2's shape with an iftype on the receiver's type in T1's default.
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().g())
class val C is T0
  new val create() => None
