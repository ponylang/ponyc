// A trait default adds 1 to x inside `iftype In <: Stringable val`, and Bar
// provides Foo[U8], where the add would work. The test expects "couldn't find
// 'add'": the trait's text is checked against In's constraint, not U8.
trait Foo[In: Any val]
  fun f(x: In): String =>
    iftype In <: Stringable val then (x + 1).string() else "o" end
class Bar is Foo[U8]
actor Main
  new create(env: Env) => env.out.print(Bar.f(4))
