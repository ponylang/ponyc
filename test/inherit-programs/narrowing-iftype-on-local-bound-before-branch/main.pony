// v is bound to mk() before the iftype, Bar fills A with U8 and Baz with
// Array[U8] val. Expects "3" then "other": the branch calling v.string() runs
// in Bar's copy and can't run in Baz's, where Array has no string().
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    let v = mk()
    iftype A <: Stringable val then v.string() else "other" end
class Bar is Foo[U8]
  fun mk(): U8 => 3
class Baz is Foo[Array[U8] val]
  fun mk(): Array[U8] val => [as U8: 1]
actor Main
  new create(env: Env) => env.out.print(Bar.f()); env.out.print(Baz.f())
