// O overrides get to return U64 and inherits label, which passes get() to
// Name. In O's label, get() is a U64, so the program prints u64.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
class val O is T
  new val create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) =>
    env.out.print(O.label())
