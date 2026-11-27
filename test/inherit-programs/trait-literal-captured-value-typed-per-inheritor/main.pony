// T.m captures get() in an object literal. C's get() returns U64 and D's
// returns None, so each class's literal sees its own type; expects u64|other.
primitive Name
  fun apply[X](x: X): String =>
    iftype X <: U64 then "u64" else "other" end
interface val S
  fun s(): String
trait T
  fun get(): (U64 | None) => None
  fun m(): S =>
    let v = get()
    object val fun s(): String => Name(v) end
class C is T
  fun get(): U64 => 1
class D is T
actor Main
  new create(env: Env) => env.out.print(C.m().s() + "|" + D.m().s())
