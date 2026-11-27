// S is T0 and narrows get() to U64; S.mk returns an object literal providing
// S. label() on C, on C.mk(), and on C.mk().mk() must all use the copy
// checked in S, where get() returns U64, printing "u64|u64|u64".
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
trait S is T0
  fun get(): U64 => 1
  fun mk(): S val => object val is S end
class val C is S
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.label() + "|" + C.mk().label() + "|" + C.mk().mk().label())
