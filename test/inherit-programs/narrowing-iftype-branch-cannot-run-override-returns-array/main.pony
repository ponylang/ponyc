// Baz fills A with (Array[U8] val | None), which is not Stringable, so f()'s
// iftype branch can't run in Baz's copy, where mk() returns an Array with no
// string(). Expects "other": the call in that branch is not an error.
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end
class Baz is Foo[(Array[U8] val | None)]
  new create() => None
  fun mk(): Array[U8] val => [as U8: 1]
actor Main
  new create(env: Env) => env.out.print(Baz.f())
