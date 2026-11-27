// Q[B2].make returns an object literal providing Foo[(B2 | None)] whose mk
// returns B2. Foo.f passes mk() to a Stringable parameter inside an iftype on
// A. The copy is checked with B2 still generic; Q[U8] must print 7 and
// Q[Array[U8] val] must print "other".
primitive Show
  fun apply(s: Stringable val): String => s.string()
trait Foo[A: Any val]
  fun mk(): A
  fun f(): String =>
    iftype A <: Stringable val then Show(mk()) else "other" end
class Q[B2: Any val]
  let _b: B2
  new create(b: B2) => _b = b
  fun make(): Foo[(B2 | None)] =>
    let b = _b
    object is Foo[(B2 | None)] fun mk(): B2 => b end
actor Main
  new create(env: Env) =>
    env.out.print(Q[U8](7).make().f())
    env.out.print(Q[Array[U8] val]([as U8: 1]).make().f())
