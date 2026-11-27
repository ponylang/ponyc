// Bar is Foo[Thing], and Thing's get() returns U64. In Bar's copy, a.get()
// in the iftype branch returns U64, so x is a U64; expects exact.
trait val HasGet
  fun get(): (U64 | None)
class val Thing
  new val create() => None
  fun get(): U64 => 1
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: U64 then "exact" else "wide" end
trait Foo[A: Any val]
  fun f(a: A): String =>
    let x = iftype A <: HasGet val then a.get() else U64(5) end
    Show(x)
class Bar is Foo[Thing]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.f(Thing))
