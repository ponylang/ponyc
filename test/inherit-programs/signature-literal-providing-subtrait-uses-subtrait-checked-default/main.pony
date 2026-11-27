// S is T0 and narrows get() to U64. M.mk returns an object literal providing
// S. Its label() must be the copy checked in S, where get() returns U64, so
// Name prints "u64"; T0's own body would print "other".
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
trait S is T0
  fun get(): U64 => 1
trait M
  fun mk(): S val => object val is S end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label())
