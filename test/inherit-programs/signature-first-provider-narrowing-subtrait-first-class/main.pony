// A named class is (S1 & S2); S1 narrows get() to U64 and S2 does not.
// K's label() must see get() as U64; expects u64.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None)
  fun label(): String => Name(get())
trait S1 is T0
  fun get(): U64
trait S2 is T0
class val K is (S1 & S2)
  new val create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) =>
    env.out.print(K.label())
