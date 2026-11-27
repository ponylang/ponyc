// An object literal provides T and overrides get with the narrower U64.
// Expects "u64": the inherited label is checked for the literal, as it is for
// a named class, so get() has the narrower type.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T
  fun get(): (U64 | None) => None
  fun label(): String => Name(get())
actor Main
  new create(env: Env) =>
    let o = object val is T fun get(): U64 => 1 end
    env.out.print(o.label())
