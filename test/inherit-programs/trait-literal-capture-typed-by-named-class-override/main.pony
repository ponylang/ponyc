// T.m's object literal captures v = get(). C overrides get to return U64, so
// in C's m, v is a U64 and the literal's n prints u64.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait U[B: Any val]
  fun n(): String => "u"
trait T
  fun get(): (U64 | None) => None
  fun m(): U[U8] val =>
    let v = get()
    object val is U[U8] fun n(): String => Name(v) end
class val C is T
  new val create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) =>
    env.out.print(C.m().n())
