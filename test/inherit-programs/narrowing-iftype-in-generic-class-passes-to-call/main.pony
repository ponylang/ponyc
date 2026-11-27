// Q[B2] provides Foo[B2]. Foo.f passes mk() to a Stringable parameter inside
// iftype A <: Stringable. The copy is checked with B2 still generic; Q[U8]
// must print 7 and Q[Array[U8] val] must print "other".
primitive Show
  fun apply(s: Stringable val): String => s.string()
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then Show(mk()) else "other" end

class Q[B2: Any val] is Foo[B2]
  let _b: B2
  new create(b: B2) => _b = b
  fun mk(): B2 => _b

actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).f())
