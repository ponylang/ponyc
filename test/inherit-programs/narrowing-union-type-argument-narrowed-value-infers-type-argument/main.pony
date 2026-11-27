// Q inherits f with A = (B2 | None), so the iftype depends on Q's B2.
// Passing mk() to Show, whose type parameter needs Stringable val, must infer
// the narrowed type and print 7 for U8 and other for Array[U8] val.
primitive Show
  fun apply[S: Stringable val](s: S): String => s.string()
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
