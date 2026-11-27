// Q[B2] provides Foo[(B2 | None)], so f()'s iftype branch, which passes mk()
// to Show as a Stringable, runs only when B2 is Stringable. Expects "7" for
// Q[U8] and "other" for Q[Array[U8] val].
primitive Show
  fun apply(s: Stringable val): String => s.string()
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then Show(mk()) else "other" end

class Q[B2: Any val] is Foo[(B2 | None)]
  let _b: B2
  new create(b: B2) => _b = b
  fun mk(): B2 => _b

actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).f())
