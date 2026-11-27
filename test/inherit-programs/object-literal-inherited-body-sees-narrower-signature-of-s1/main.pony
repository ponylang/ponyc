// An object literal provides S2 & S1, where S1 narrows get() to U64, and
// inherits label, which passes get() to Name. Expects "u64": label is checked
// for the literal, where get() returns U64.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait T0
  fun get(): (U64 | None)
  fun label(): String => Name(get())
trait S1 is T0
  fun get(): U64
trait S2 is T0
actor Main
  new create(env: Env) =>
    let o = object val is (S2 & S1) fun get(): U64 => 1 end
    env.out.print(o.label())
