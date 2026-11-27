// A trait literal is (T0 & S1). label() comes from T0's own body, the first
// provider, so get() is (U64 | None); expects other.
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
  fun mk(): T0 val => object val is (T0 & S1) fun get(): U64 => 1 end
class val C is M
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label())
