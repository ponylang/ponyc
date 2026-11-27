// Bar fills A with Array[U8] val, so f()'s iftype branch, which binds locals
// of type A and calls string() on them, can't run in Bar's copy. Expects
// "other": Array having no string() is not an error in that branch.
trait Foo[A: Any val]
  fun mk(): A
  fun f(x: A): String =>
    iftype A <: Stringable val then
      let y: A = mk()
      let z: A = x
      y.string() + z.string()
    else "other" end
class Bar is Foo[Array[U8] val]
  fun mk(): Array[U8] val => [as U8: 1]
actor Main
  new create(env: Env) => env.out.print(Bar.f([as U8: 2]))
