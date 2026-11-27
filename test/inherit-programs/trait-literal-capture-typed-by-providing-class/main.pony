// T.m() captures v = get() in a U literal. C overrides get() to return U64,
// so in C's copy of m() v is a U64 and Name(v) prints "u64". The captured
// local takes its type from the class the body was copied into.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
trait U[B: Any val]
  fun n(): String => "u"
trait T[A: Any val]
  fun get(): (U64 | None) => None
  fun m(): U[Array[A] val] val =>
    let v = get()
    object val is U[Array[A] val] fun n(): String => Name(v) end
class val C is T[U8]
  new val create() => None
  fun get(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(C.m().n())
