// The generic-class version: in Bar[Thing], a.get() is typed by HasGet's
// (U64 | None), so x is not just U64; expects wide.
trait val HasGet
  fun get(): (U64 | None)
class val Thing
  new val create() => None
  fun get(): U64 => 1
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: U64 then "exact" else "wide" end
class Bar[A: Any val]
  new create() => None
  fun f(a: A): String =>
    let x = iftype A <: HasGet val then a.get() else U64(5) end
    Show(x)
actor Main
  new create(env: Env) => env.out.print(Bar[Thing].f(Thing))
