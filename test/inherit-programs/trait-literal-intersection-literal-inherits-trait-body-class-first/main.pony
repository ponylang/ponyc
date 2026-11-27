// T0.fa returns a literal of (T1 & M), which inherits T1.g. g passes this
// to Name, and in T1's body this is T1 val, which is not M, so the program
// must print notM even with the classes declared before the traits.
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().g())
class val C is T0
  new val create() => None
primitive Name
  fun apply[X](x: X): String => iftype X <: M val then "M" else "notM" end
trait val M
trait T0
  fun val fa(): T1 val => object val is (T1 & M) end
trait T1
  fun val g(): String => Name(this)
