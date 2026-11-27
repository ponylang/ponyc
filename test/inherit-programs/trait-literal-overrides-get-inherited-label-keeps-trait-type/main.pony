// T.mk's object literal overrides get to return U64 and inherits label, which
// passes get() to Name. In the inherited label, get() keeps T's (U64 | None)
// type, so both calls print other.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
  fun mk(): T val => object val is T fun get(): U64 => 1 end
class val C is T
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.mk().label() + "|" + C.mk().mk().label())
