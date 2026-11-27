// An object literal provides T and overrides get with the narrower U64. Its
// copy of m captures get()'s result in a nested literal. Expects "u64": the
// captured value has the narrower type.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait U
  fun n(): String => "u"
trait T
  fun get(): (U64 | None) => None
  fun m(): U val =>
    let v = get()
    object val is U fun n(): String => Name(v) end
actor Main
  new create(env: Env) =>
    let o = object val is T fun get(): U64 => 1 end
    env.out.print(o.m().n())
