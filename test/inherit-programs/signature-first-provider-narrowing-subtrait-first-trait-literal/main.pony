// A trait literal is (S1 & S2); S1 narrows get() to U64 and S2 does not.
// label() comes from S1's copy, the first provider; expects u64.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None)
  fun label(): String => Name(get())
trait S1 is T0
  fun get(): U64
trait S2 is T0
trait M
  fun mk(): T0 val => object val is (S1 & S2) fun get(): U64 => 1 end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label())
