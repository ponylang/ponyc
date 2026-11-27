// Foo.f binds x from an iftype whose untaken branch makes a lambda and calls
// mk(). Bar narrows mk's result to U64, so x is U64 in Bar's copy and Show
// prints "exact"; the lambda must not change that.
primitive Show
  fun apply[T: Any val](t: T): String =>
    iftype T <: U64 then "exact" else "wide" end
trait Foo[A: Any val]
  fun mk(): (U64 | None)
  fun f(): String =>
    let x = iftype A <: U8 then
      let g = {(): U64 => 3} val
      mk()
    else U64(5) end
    Show(x)
class Bar is Foo[String]
  new create() => None
  fun mk(): U64 => 1
actor Main
  new create(env: Env) => env.out.print(Bar.f())
